// Asgn 4: A simple HTTP server.
// Starter Code by:  Andi Quinn
// Final Code by:    Anthony Reyna

#include "connection.h"
#include "listener_socket.h"
#include "request.h"
#include "response.h"
#include "queue.h"
#include "rwlock.h"
#include "lockmap.h"

#include <pthread.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#define QUEUE_SIZE  64
#define LOCKMAP_SIZE  1024
lockmap_t *lockmap;   //hashmap of rw_lock's
pthread_mutex_t log_mutex;
//rwlock_t *RW_LOCK;

void handle_connection(int);

void *thread_func(void *arg);

void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);
void handle_unknown(conn_t *conn);

int main(int argc, char **argv) {
  size_t num_of_threads = 4;   // default

  //RW_LOCK = rwlock_new(WRITERS, 0);

  // Handle user designating size of thread pool
  int opt;
  while ((opt = getopt(argc, argv, "t:")) != -1) {
    switch(opt) {
      case 't':
        num_of_threads = (size_t)strtoull(optarg, NULL, 10);
        break;

        default:
          fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
          return EXIT_FAILURE;
    }
  }
  if (num_of_threads == 0) {
    fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Ensure presence of port number
  if (optind >= argc) {
    fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Extract port number
  char *endptr = NULL;
  size_t port = (size_t)strtoull(argv[optind], &endptr, 10);
  if (endptr && *endptr != '\0') {
    warnx("invalid port number: %s", argv[optind]);
    return EXIT_FAILURE;
  }

  // Ensure valid port number
  if (port < 1 || port > 65535) {
    warnx("invalid port number: %s", argv[optind]);
    return EXIT_FAILURE;
  }

  // Create listener socket
  signal(SIGPIPE, SIG_IGN);
  Listener_Socket_t *sock = ls_new(port);
  if (!sock) {
    warnx("cannot open socket");
    return EXIT_FAILURE;
  }

  // Create our queue for connections
  queue_t *server_queue = queue_new(QUEUE_SIZE);

  // create our thread-pool of workers
  pthread_t *workers = malloc(num_of_threads * sizeof(pthread_t));
  if (!workers) {
    perror("worker malloc");
    exit(EXIT_FAILURE);
  }
  for (size_t i = 0; i < num_of_threads; ++i) {
    if (pthread_create(&workers[i], NULL, thread_func, server_queue) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
  }

  // create hashmap of rw_lock's
  lockmap = lockmap_create(LOCKMAP_SIZE);
  if (lockmap == NULL) {
    fprintf(stderr, "Failed to create lockmap\n");
    exit(1);
  }

  // init mutex for the Audit Log
  pthread_mutex_init(&log_mutex, NULL);

  // Dispatcher thread places connections in the queue for worker threads to handle
  while (1) {
    int connfd = ls_accept(sock);
    if (connfd < 0) {
      perror("accept: connfd < 0");
      continue;
    }
    int *fd = malloc(sizeof(int));
    if (!fd) {
      perror("fd malloc");
      exit(EXIT_FAILURE);
    }

    *fd = connfd;
    queue_push(server_queue, fd);
  }

  // Clean and exit
  queue_delete(&server_queue);
//  rwlock_delete(&RW_LOCK);
  // TODO :: server_queue = NULL ?
  ls_delete(&sock);
  free(workers);
  lockmap_destroy(lockmap);

  return EXIT_SUCCESS;
}

void *thread_func(void *arg) {
  queue_t *server_q = (queue_t *)arg;
  void *elem;

  while (1) {
    if (!queue_pop(server_q, &elem)) {
      // TODO :: how do they expect this to be handled?
      continue;
    } else {
      int *fd = elem;
      handle_connection(*fd);
      close(*fd);
      free(fd);
    }
  }

  return NULL;
}

void handle_connection(int connfd) {

  conn_t *conn = conn_new(connfd);

  const Response_t *res = conn_parse(conn);

  if (res != NULL) {
    conn_send_response(conn, res);
  } else {
    printf("%s", conn_str(conn));
    const Request_t *req = conn_get_request(conn);
    if (req == &REQUEST_PUT) {
      handle_put(conn);
    }
    else if (req == &REQUEST_GET) {
      handle_get(conn);
    }
    else if (req == &REQUEST_UNSUPPORTED) {
      handle_unsupported(conn);
    }
    else {
      // TODO :: how do they expect this to be handled?
      handle_unknown(conn);
    }
  }

  conn_delete(&conn);
}


void handle_put(conn_t *conn) {
  // pieces to build audit log entry later
  char log_msg[1024];
  uint16_t code;
  char *req_id;
  const Response_t *res = NULL;

  char *uri = conn_get_uri(conn);

  // Receive body into a temp file WITHOUT holding any lock.
  // prevents blocking other writers while waiting for slow/paused clients.
  char tempname[] = ".tmp_XXXXXX";
  int tempfd = mkstemp(tempname);
  if (tempfd < 0) {
    res = &RESPONSE_INTERNAL_SERVER_ERROR;
    goto out;
  }

  res = conn_recv_file(conn, tempfd);
  close(tempfd);

  if (res != NULL) {
    // Error receiving body (e.g., bad request)
    unlink(tempname);
    goto out;
  }

  // rename temp file to target
  {
    rwlock_t *rwlock = lockmap_get_lock(lockmap, uri);
    writer_lock(rwlock);

    bool existed = access(uri, F_OK) == 0;

    if (rename(tempname, uri) != 0) {
      writer_unlock(rwlock);
      unlink(tempname);
      if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
        res = &RESPONSE_FORBIDDEN;
      } else {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
      }
      goto out;
    }

    writer_unlock(rwlock);
    res = existed ? &RESPONSE_OK : &RESPONSE_CREATED;
  }

out:
  // Build message for audit log entry
  code = response_get_code(res);
  req_id = conn_get_header(conn, "Request-Id");
  snprintf(log_msg, sizeof(log_msg), "%s,%s,%u,%s\n", "PUT", uri, code, req_id);

  // write to audit log with lock
  pthread_mutex_lock(&log_mutex);
  fprintf(stderr, "%s", log_msg);
  pthread_mutex_unlock(&log_mutex);
  conn_send_response(conn, res);
}

void handle_get(conn_t *conn) {
  // pieces to build audit log entry later
  char log_msg[1024];
  uint16_t code;
  char *req_id;
  const Response_t *res = NULL;
  bool file_sent = false;

  char *uri = conn_get_uri(conn);

  // Entering critical section w.r.t. URI, so obtain lock
  rwlock_t *rwlock = lockmap_get_lock(lockmap, uri);
  reader_lock(rwlock);
  // Open the file
  int fd = open(uri, O_RDONLY);

  if (fd < 0) {
    reader_unlock(rwlock);
    if (errno == ENOENT) {
        res = &RESPONSE_NOT_FOUND;
    } else if (errno == EACCES) {
        res = &RESPONSE_FORBIDDEN;
    } else {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
    }
    goto out;
  }

  // Check if file exists but is not a regular file
  struct stat st;

  if (fstat(fd, &st) < 0) {
      res = &RESPONSE_INTERNAL_SERVER_ERROR;
      close(fd);
      reader_unlock(rwlock);
      goto out;
  }

  if (!S_ISREG(st.st_mode)) {
      res = &RESPONSE_FORBIDDEN;
      close(fd);
      reader_unlock(rwlock);
      goto out;
  }

  /* fd is valid */
  // send the file
  res = conn_send_file(conn, fd, st.st_size);
  close(fd);
  reader_unlock(rwlock);

  if (res == NULL) {
    res = &RESPONSE_OK;
    file_sent = true;
  }

out:
  // Build message for audit log entry
  code = response_get_code(res);
  req_id = conn_get_header(conn, "Request-Id");
  snprintf(log_msg, sizeof(log_msg), "%s,%s,%u,%s\n", "GET", uri, code, req_id);

  // write to audit log with lock
  pthread_mutex_lock(&log_mutex);
  fprintf(stderr, "%s", log_msg);
  pthread_mutex_unlock(&log_mutex);

  if (!file_sent) {
    conn_send_response(conn, res);
  }
}

void handle_unsupported(conn_t *conn) {
  // TODO :: Do we distinguish between possible errors here?
  //   e.g. bad_req, RESPONSE_VERSION_NOT_SUPPORTED, 
  const Response_t *res = &RESPONSE_NOT_IMPLEMENTED;
  conn_send_response(conn, res);
}

void handle_unknown(conn_t *conn) {
  // TODO :: Do we distinguish between possible errors here?
  //   e.g. bad_req, RESPONSE_VERSION_NOT_SUPPORTED, 
  const Response_t *res = &RESPONSE_BAD_REQUEST;
  conn_send_response(conn, res);
}
