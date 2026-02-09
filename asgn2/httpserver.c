/**
 * @File httpserver.c
 *
 * This file contains the main function for the HTTP server.
 *
 * @author [Anthony Reyna]
 */

#include "listener_socket.h"
#include "iowrapper.h"
#include "debug.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>

#include <stdbool.h>

// potentially ^ to beginning
#define REQUEST_LINE_REGEX "(" TYPE_REGEX ") (" URI_REGEX ") (" HTTP_REGEX ")\r\n"
#define KEY_VALUE_REGEX    "(" HEADER_FIELD_REGEX "): (" HEADER_VALUE_REGEX ")\r\n"
#define KEY_REGEX          "^(" HEADER_FIELD_REGEX ":)"
#define VALUE_REGEX        "(" HEADER_VALUE_REGEX ")\r\n"
#define HEADER_LINE_REGEX  "^(" HEADER_FIELD_REGEX "):[ ]*(" HEADER_VALUE_REGEX ")\r\n"

bool g_debug = false;

#define BUFFER_SIZE 4096
#define HEADER_MAX  2048

/*----- HELPERS -----*/
void listener_init(Listener_Socket_t **sock, int port);
char *read_until_rnrn(char *buffer, size_t *buf_len, int connfd);

char *extract_request_field(const char *pattern, const char *buffer, size_t offset, size_t buf_len);
char *extract_request_field_group(
    const char *pattern, const char *buffer, size_t offset, size_t buf_len, size_t group_idx);

void handle_get_request(char *buffer, char *uri, int connfd);
void handle_put_request(
    char *buffer, char *uri, int connfd, size_t *buf_len, size_t rnrn_pos, char *key_values);

void handle_unimplemented_request(int connfd);
void handle_invalid_version(int connfd);
void handle_unsupported_version(int connfd);
void handle_file_not_found(int connfd);
void handle_forbidden_file(int connfd);

void handle_bad_request(int connfd);
void handle_internal_error(int connfd);

void put_ok_response(int connfd);
void put_creation_response(int connfd);
long get_content_length(char *header_field_list);

off_t file_size(int fd);

static void send_and_close(int connfd, const char *resp) {
    (void) write(connfd, resp, strlen(resp)); // ignore write errors here
    shutdown(connfd, SHUT_RDWR);
    close(connfd);
}

/** @brief Handles a connection from a client.
 *
 *  @param connfd The file descriptor for the connection.
 *
 *  @return void
 */
void handle_connection(int connfd) {

    char buffer[BUFFER_SIZE];
    size_t buf_len = 0; // number of total bytes currently in buffer
    //size_t buf_pos = 0; // marks the index where unused bytes begin

    // read until buffer contains \r\n\r\n ;  buffer may include subsequent message
    char *rnrn_pos = read_until_rnrn(buffer, &buf_len, connfd);
    // TODO :: Do we need to handle instances of no \r\n\r\n ?
    // if so, then here or from within read_until_rnrn() ?
    if (!rnrn_pos) {
        handle_bad_request(connfd);
        return;
    }

    // print entire buffer for debugging
    // fprintf(stderr, "Buffer contents after reading until \\r\\n\\r\\n:\n");
    // for (size_t i = 0; i < buf_len; i++) {
    //     fprintf(stderr, "%c", buffer[i]);
    // }

    size_t request_len = (rnrn_pos - buffer) + 4;
    char *request = malloc(request_len + 1);
    if (!request) {
        handle_internal_error(connfd);
        return;
    }

    fprintf(stderr, "Request length: %zu\n", request_len);
    fprintf(stderr, "Request contents:\n");
    for (size_t i = 0; i < request_len; i++) {
        fprintf(stderr, "%c", buffer[i]);
    }

    memcpy(request, buffer, request_len);
    request[request_len] = '\0';
    fprintf(stderr, "Copied request contents to separate string:\n%s\n", request);

    // Use regular expressions to parse request line
    //char *request_type = extract_request_field("(" TYPE_REGEX ")", headers, 0, header_len);
    //char *uri_tmp      = extract_request_field("(" URI_REGEX ")", headers, 0, header_len);
    //char *http_version = extract_request_field("(" HTTP_REGEX ")\r\n", headers, 0, header_len);

    char *request_type
        = extract_request_field_group(REQUEST_LINE_REGEX, request, 0, request_len, 1);
    char *uri_tmp = extract_request_field_group(REQUEST_LINE_REGEX, request, 0, request_len, 2);
    char *http_version
        = extract_request_field_group(REQUEST_LINE_REGEX, request, 0, request_len, 3);

    char *uri = uri_tmp;
    if (uri && uri[0] == '/') {
        uri++; // skip leading '/'
    }
    /*    
    //char *request_type = extract_request_field("(" TYPE_REGEX ")", headers, 0, header_len);
    //buffer[strlen(request_type)] = '\0'; // null-terminate buffer at end of request type
    //buf_pos += strlen(request_type)
    //           + 1; // move buf_pos to index of first char after request type (skip \0)
    //fprintf(stderr, "Request type: %s\n", request_type);
    //char *uri_tmp = extract_request_field( "(" URI_REGEX ")", headers, 0, header_len); // +1 to skip \0 after request type
    //fprintf(stderr, "URI with preceding slash: %s\n", uri_tmp);
    //buffer[strlen(request_type) + strlen(uri_tmp) + 1]
    //    = '\0'; // null-terminate buffer at end of URI
    //char *uri = uri_tmp + 1; // skip preceding '/'
    //buf_pos += strlen(uri_tmp) + 1; // move buf_pos to index of first char after URI (skip \0)
    //fprintf(stderr, "URI: %s\n", uri);
    //char *http_version = extract_request_field("(" HTTP_REGEX ")\r\n", headers, 0, header_len);
    //fprintf(stderr, "HTTP version: %s\n", http_version);

    //fprintf(
    //    stderr, "Request type: %s\nURI: %s\nHTTP Version: %s\n", request_type, uri, http_version);
    //if (header_len != buf_pos) {
    //    fprintf(stderr, "Warning: math not adding up. buf_pos: %zu, header_len: %zu\n", buf_pos,
    //        header_len);
    //}
*/
    if (!http_version) {
        handle_invalid_version(connfd);
    } else if (!request_type || !uri) {
        handle_bad_request(connfd);
    } else if (strcmp(http_version, HTTP_VERSION) != 0) {
        handle_unsupported_version(connfd);
    } else if (strcasecmp(request_type, "GET") == 0) {
        handle_get_request(buffer, uri, connfd);
    } else if (strcasecmp(request_type, "PUT") == 0) {
        char *header_list = strstr(request, "\r\n");
        if (!header_list) {
            fprintf(stderr, "Could not find end of request line\n");
            handle_bad_request(connfd);
            goto cleanup;
        }
        header_list += 2; // index of first byte of header fields (after request line + \r\n)
        size_t rnrn_offset = (size_t) (rnrn_pos - buffer);
        //char *key_values = headers + request_line_len; // index of first byte of header fields (after request line)
        handle_put_request(buffer, uri, connfd, &buf_len, rnrn_offset, header_list);
    } else {
        handle_unimplemented_request(connfd);
    }
    //  PUT /temp.txt HTTP/1.1\r\n
    //                         ^
    //                         |
    //                         +-- header_list
    //  Host: localhost:32768\r\n
    //  ^
    //  |
    //  +-- header_list += 2 (to skip \r\n at end of request line)
    //  User-Agent: curl/8.5.0\r\n
    //  Accept: */*\r\n
    //  Content-Length: 1803\r\n\r\n\0
    //                       ^      ^
    //                       |      |
    //                       |      +-- request_len = rnrn_pos - buffer + 4
    //                       |
    //                       +---- rnrn_pos

    //printf("buffer unread starts at: %c %c %c %c\n", buffer[i-1], buf_pos[i], buf_pos[i+1], buf_pos[i+2]);
    //printf("last few bytes: %c %c %c \n", buffer[bytes_read-3], buffer[bytes_read-2], buffer[bytes_read-1]);

cleanup:
    free(request);
    free(request_type);
    free(uri_tmp);
    free(http_version);
    fprintf(stderr, "Free!\n");
    fprintf(stderr, "Finished handling request\n");
    //exit(EXIT_SUCCESS);
    return;
}

/** @brief Main function for the HTTP server.
 *
 *  @param argc The number of arguments.
 *  @param argv The arguments.
 *
 *  @return EXIT_SUCCESS if successful, EXIT_FAILURE otherwise.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[1], &endptr, 10);

    // Ensure valid port number
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    // TODO :: handle this error (errno?) as well as CTRL-C?
    // I think this already satisfies requirements
    signal(SIGPIPE, SIG_IGN);

    Listener_Socket_t *sock = NULL;
    listener_init(&sock, port);

    while (1) {
        int connfd = ls_accept(sock);
        if (connfd < 0) {
            if (errno == EINTR)
                continue;
            continue;
        }
        handle_connection(connfd);
        close(connfd);
        // ls_delete(&sock);
        // return EXIT_SUCCESS;
    }

    ls_delete(&sock);
    return EXIT_SUCCESS;
}

// TODO :: special instances of erroneous requests
void handle_get_request(char *buffer, char *uri, int connfd) {
    // Attempt to open the requested file
    int rfd = open(uri, O_RDONLY);
    fprintf(stderr, "URI: %s\n", uri);
    fprintf(stderr, "Open returned rfd: %d, ENOENT=%d\n", rfd, ENOENT);

    // If open failed, respond to client with appropriate msg
    if (rfd < 0) {
        switch (errno) {
        case ENOENT:
            // TODO :: do i keep this?
            // file does not exist
            handle_file_not_found(connfd);
            return;
        case EACCES:
            // TODO :: do i keep this?
            // exists, but not accessible (permissions)
            handle_forbidden_file(connfd);
            return;
        case EISDIR:
            // TODO :: do i keep this?
            // is a directory, not a regular file
            handle_file_not_found(connfd);
            return;
        default:
            // TODO :: are there other errors?
            fprintf(stderr, "Unknown file-open error\n");
            send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Length: 3\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Bad");
            break;
        }
    }

    // Get file size for Content-Length header
    struct stat st;
    if (fstat(rfd, &st) < 0) {
        fprintf(stderr, "fstat error\n");
        close(rfd);
        handle_file_not_found(connfd);
        return;
    }
    off_t fsize = st.st_size;

    // Check if file is regular file
    if (!S_ISREG(st.st_mode)) {
        close(rfd);
        handle_forbidden_file(connfd);
        return;
    }

    // Build response header
    char *response = NULL;
    size_t response_len = snprintf(NULL, 0,
        HTTP_VERSION " 200 OK\r\n"
                     "Content-Length: %ld\r\n\r\n",
        fsize);
    response = malloc(response_len + 1);
    snprintf(response, response_len + 1,
        HTTP_VERSION " 200 OK\r\n"
                     "Content-Length: %ld\r\n\r\n",
        fsize);
    response[response_len] = '\0';

    // Send response header to client
    write_n_bytes(connfd, response, response_len);
    free(response);

    // Call read and write until EOF
    while (1) {
        ssize_t bytes_read = read_n_bytes(rfd, buffer, BUFFER_SIZE);
        if (bytes_read == 0) {
            break;
        } else if (bytes_read < 0) {
            send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Length: 3\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Bad");
            if (errno == EINTR) {
                continue; // interrupted, retry
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket receive timed out (5s timeout from ls_accept)
                // timeout
                close(rfd);
                handle_bad_request(connfd);
                return;
            }
            close(rfd);
            handle_bad_request(connfd);
            return;
        }

        ssize_t bytes_written = write_n_bytes(connfd, buffer, bytes_read);
        if (bytes_written < 0) {
            fprintf(stderr, "Error writing file\n");
            send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Length: 3\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Bad");
        }
    }

    close(rfd);
    return;
}

void handle_put_request(
    char *buffer, char *uri, int connfd, size_t *buf_len, size_t rnrn_pos, char *key_values) {
    //if (!request_line_len) {
    //    fprintf(stderr, "Request line length is 0\n");
    //    handle_bad_request(connfd);
    //    return;
    //}

    g_debug = true;
    bool content_length_found = false;
    size_t content_length = 0;
    size_t key_values_pos = 0;
    size_t kv_len = strlen(key_values); // length of header fields section (after request line)
    fprintf(stderr, "header_len: %zu\n", kv_len);
    //fprintf(stderr, "request_line_len: %zu\n", request_line_len);
    fprintf(stderr, "key_values_pos starts at: %zu\n", key_values_pos);
    fprintf(stderr, "key_values length: %zu\n", kv_len);

    //  PUT /temp.txt HTTP/1.1\r\n
    //                         ^
    //                         |
    //                         +-- header_list
    //  Host: localhost:32768\r\n
    //  ^
    //  |
    //  +-- header_list += 2 (to skip \r\n at end of request line)
    //  User-Agent: curl/8.5.0\r\n
    //  Accept: */*\r\n
    //  Content-Length: 1803\r\n\r\n\0
    //                       ^      ^
    //                       |      |
    //                       |      +-- request_len = rnrn_pos - buffer + 4
    //                       |
    //                       +---- rnrn_pos
    // kv len, req_line_line,

    // look for Content-Length header-field key and value while validating header fields with regex
    while (key_values_pos + 2 < kv_len) {
        if (key_values[key_values_pos] == '\r' && key_values[key_values_pos + 1] == '\n') {
            fprintf(stderr, "Reached end of header fields without finding Content-Length header\n");
            key_values_pos += 2; // move past \r\n
            break;
        }

        // Find end of this header line
        char *line_end = strstr(key_values + key_values_pos, "\r\n");
        if (!line_end) {
            fprintf(
                stderr, "Could not find end of header line starting at pos: %zu\n", key_values_pos);
            //close(wfd);
            handle_bad_request(connfd);
            return;
        }
        size_t line_len = (size_t) (line_end - (key_values + key_values_pos)) + 2; // include "\r\n"

        // Make a NUL-terminated copy of just this line for regex matching
        char *line = malloc(line_len + 1);
        if (!line) {
            //close(wfd);
            handle_internal_error(connfd);
            return;
        }
        memcpy(line, key_values + key_values_pos, line_len);
        line[line_len] = '\0';

        fprintf(stderr, "\nExtracting header key from:\n%s", key_values + key_values_pos);
        fprintf(stderr, "More specifically, from:\n%s\n", line);

        //char *header_key = extract_request_field(KEY_REGEX, key_values, key_values_pos, strlen(key_values));
        char *header_key = extract_request_field_group(HEADER_LINE_REGEX, line, 0, line_len, 1);
        char *header_value = extract_request_field_group(HEADER_LINE_REGEX, line, 0, line_len, 2);
        free(line);

        if (!header_key || !header_value) {
            fprintf(stderr, "Header Key or Value not found\n");
            free(header_key);
            free(header_value);
            //close(wfd);
            handle_bad_request(connfd);
            return;
        }
        fprintf(stderr, "Extracted header key: %s\n", header_key);
        fprintf(stderr, "Extracted header value: %s\n", header_value);

        fprintf(stderr, "\nChecking if header key is Content-Length...\n");
        if (strcasecmp(header_key, "Content-Length") == 0) {
            char *content_length_str = header_value;
            content_length = strtoul(content_length_str, NULL, 10);
            content_length_found = true;
            fprintf(stderr, "Found Content-Length header with value: %zu\n", content_length);
        } else {
            fprintf(stderr, "\nHeader key was not Content-Length, incrementing key_values_pos\n");
        }
        free(header_key);
        free(header_value);
        key_values_pos += line_len; // move to start of next header line
    }

    if (!content_length_found) {
        fprintf(stderr, "Content-Length header not found\n");
        //close(wfd);
        handle_bad_request(connfd);
        return;
    }

    // Check if file exists but is not a regular file
    struct stat st;
    bool existed = (stat(uri, &st) == 0);
    if (existed && !S_ISREG(st.st_mode)) {
        handle_forbidden_file(connfd);
        return;
    }

    // Create file if it doesn't exist
    //wfd = open(uri, O_WRONLY | O_TRUNC | O_CREAT | O_EXCL, 0644);
    int wfd = open(uri, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (wfd < 0) {
        if (errno == EACCES || errno == EPERM || errno == EISDIR) {
            handle_forbidden_file(connfd);
        } else {
            handle_file_not_found(connfd);
        }
        return;
    }

    size_t bytes_requested = content_length;
    fprintf(stderr, "Content length converted to size_t bytes_requested = %zu\n", bytes_requested);

    //  PUT /temp.txt HTTP/1.1\r\n
    //                         ^
    //                         |
    //                         +-- header_list
    //  Host: localhost:32768\r\n
    //  ^
    //  |
    //  +-- header_list += 2 (to skip \r\n at end of request line)
    //  User-Agent: curl/8.5.0\r\n
    //  Accept: */*\r\n
    //  Content-Length: 1803\r\n\r\n\0
    //                       ^      ^
    //                       |      |
    //                       |      +-- request_len = rnrn_pos - buffer + 4
    //                       |
    //                       +---- rnrn_pos
    // kv len, req_line_line,

    ssize_t payload_bytes_in_buffer = *buf_len - (rnrn_pos + 4); // +4 to account for \r\n\r\n
    fprintf(stderr, "payload_bytes_in_buffer: %zu\n", payload_bytes_in_buffer);

    if (payload_bytes_in_buffer > 0) {

        ssize_t bytes_to_write = (payload_bytes_in_buffer < (ssize_t) bytes_requested)
                                     ? payload_bytes_in_buffer
                                     : bytes_requested;
        ssize_t bytes_written = write_n_bytes(wfd, buffer + (rnrn_pos + 4), bytes_to_write);
        if (bytes_written < 0) {
            send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Length: 3\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Bad");
        }
        bytes_requested -= bytes_written;
    }
    //bytes_requested -= write_n_bytes(wfd, payload_pos, payload_bytes_in_buffer);

    fprintf(
        stderr, "Bytes requested after accounting for payload in buffer: %zu\n", bytes_requested);
    while (bytes_requested > 0) {
        fprintf(stderr, "bytes currently in bufffer: %zu\n", *buf_len);
        for (size_t i = 100; i < *buf_len; i++) {
            fprintf(stderr, "%c", buffer[i]);
        }
        size_t to_read = bytes_requested < BUFFER_SIZE ? bytes_requested : BUFFER_SIZE;
        ssize_t bytes_read = read_n_bytes(connfd, buffer, to_read);
        if (bytes_read == 0) {
            break;
        } else if (bytes_read < 0) {
            send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Length: 3\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Bad");
            if (errno == EINTR) {
                continue; // interrupted, retry
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket receive timed out (5s timeout from ls_accept)
                // timeout
                close(wfd);
                handle_bad_request(connfd);
                return;
            }
            close(wfd);
            handle_bad_request(connfd);
            return;
        }

        fprintf(stderr, "Read_n_bytes read: %zd bytes\n", bytes_read);
        //if (bytes_read == 0) {
        //    continue; // EOF from client before we got all bytes, but keep waiting for more (until timeout)
        //}
        ssize_t bytes_to_write
            = (bytes_read < (ssize_t) bytes_requested) ? bytes_read : bytes_requested;
        fprintf(stderr, "Bytes to write from this read: %zu\n", bytes_to_write);
        ssize_t bytes_written = write_n_bytes(wfd, buffer, bytes_to_write);
        if (bytes_written < 0) {
            send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Length: 3\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Bad");
        }
        fprintf(stderr, "Bytes actually written from this read: %zd\n", bytes_written);
        bytes_requested -= bytes_written;
    }
    fprintf(stderr, "Finished writing payload. Remaining bytes requested (should be 0): %zu\n",
        bytes_requested);
    //    size_t bytes_to_write = bytes_requested - (*buf_len - (payload_pos - buffer));
    //   fprintf(stderr, "bytes to write: %ld\n", bytes_to_write);
    //fprintf(stderr, "Payload: %c\n%c\n%c\n%c\n", payload_pos[8], payload_pos[9], payload_pos[10], payload_pos[11]);
    //fprintf(stderr, "rnrn_pos: %c\n%c\n%c\n%c\n", (*rnrn_pos)[8], (*rnrn_pos)[9], (*rnrn_pos)[10], (*rnrn_pos)[11]);
    // fprintf(stderr, "Buffer_len: %ld\n", *buf_len);
    //fprintf(stderr, "buffer end: %c\n%c\n%c\n%c\n", buffer[*buf_len-8], buffer[*buf_len-7], buffer[*buf_len-6], buffer[*buf_len-5]);

    //for (size_t i = 0; i < *buf_len; i++) {
    //    fprintf(stderr, "buffer[%zu]: %c\n", i, buffer[i]);
    //}
    fprintf(stderr, "existed flag: %d\n", existed);

    if (existed) {
        put_ok_response(connfd); // 200
    } else {
        put_creation_response(connfd); // 201
    }
    close(wfd);
}

char *extract_request_field_group(
    const char *pattern, const char *buffer, size_t offset, size_t buf_len, size_t group_idx) {
    if (offset > buf_len)
        return NULL;

    size_t slice_len = buf_len - offset;
    char *slice = malloc(slice_len + 1);
    if (!slice)
        return NULL;
    memcpy(slice, buffer + offset, slice_len);
    slice[slice_len] = '\0';

    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        free(slice);
        return NULL;
    }

    // allocate enough matches: whole match + up to group_idx
    size_t nmatch = group_idx + 1;
    regmatch_t *pmatch = calloc(nmatch, sizeof(*pmatch));
    if (!pmatch) {
        regfree(&regex);
        free(slice);
        return NULL;
    }

    int rc = regexec(&regex, slice, nmatch, pmatch, 0);
    regfree(&regex);

    if (rc != 0) {
        free(pmatch);
        free(slice);
        return NULL;
    }

    if (pmatch[group_idx].rm_so < 0 || pmatch[group_idx].rm_eo < 0
        || pmatch[group_idx].rm_eo < pmatch[group_idx].rm_so) {
        free(pmatch);
        free(slice);
        return NULL;
    }

    size_t len = (size_t) (pmatch[group_idx].rm_eo - pmatch[group_idx].rm_so);
    char *out = malloc(len + 1);
    if (!out) {
        free(pmatch);
        free(slice);
        return NULL;
    }
    memcpy(out, slice + (size_t) pmatch[group_idx].rm_so, len);
    out[len] = '\0';

    free(pmatch);
    free(slice);
    return out;
}

char *extract_request_field(
    const char *target_pattern, const char *buffer, size_t offset, size_t buf_len) {
    if (offset > buf_len) {
        fprintf(stderr, "Offset exceeds buffer length\n");
        return NULL;
    }

    //if (g_debug) {
    //    fprintf(stderr, "Extracting field with pattern: %s\nfrom buffer starting at offset: %zu\n", target_pattern, offset);
    //for (size_t i = 0; i < 5; i++) {
    //    fprintf(stderr, "buffer[%zu]: %c\n", offset + i, buffer[offset + i]);
    //}
    //}

    //fprintf(stderr, "Extracting field with pattern: %s\nfrom buffer starting at offset: %zu\n", target_pattern, offset);
    //    for (size_t i = 0; i < 5; i++) {
    //    fprintf(stderr, "buffer[%zu]: %c\n", offset + i, buffer[offset + i]);
    //}

    // Make a NUL-terminated copy of the slice we are searching over, starting at offset
    size_t slice_len = buf_len - offset;
    char *slice = malloc(slice_len + 1);
    if (!slice) {
        fprintf(stderr, "Failed to allocate memory for slice\n");
        return NULL;
    }
    memcpy(slice, buffer + offset, slice_len);
    slice[slice_len] = '\0';

    // Compile regular expression
    regex_t regex;
    int rc = regcomp(&regex, target_pattern, REG_EXTENDED);
    if (rc != 0) {
        fprintf(stderr, "Regex compilation failed with code: %d\n", rc);
        free(slice);
        return NULL;
    }

    // pmatch holds indices of matched group
    regmatch_t pmatch[2];

    if (g_debug) {
        fprintf(stderr, "About to regexec starting from offset: %zu\n", offset);
        //fprintf(stderr, "About to regexec starting from offset: %c%c\n", buffer[offset], buffer[offset+1]);
    }

    // execute regex search
    int match = regexec(&regex, slice, 2, pmatch, 0);
    //fprintf(stderr, "Finished regexec. Match code: %d\n", match);
    regfree(&regex); // free memory used for regex compilation

    //fprintf(stderr, "After regfree\n");
    if (match != 0) {
        fprintf(stderr, "Regex execution failed with code: %d\n", match);
        free(slice);
        return NULL;
    }
    //fprintf(stderr, "regexec succeeded. pmatch[0] (full match) indices: rm_so=%d, rm_eo=%d\n",
    //    pmatch[0].rm_so, pmatch[0].rm_eo);

    // ensure capture group 1 exists
    if (pmatch[1].rm_so < 0 || pmatch[1].rm_eo < 0 || pmatch[1].rm_eo < pmatch[1].rm_so) {
        //fprintf(stderr, "Invalid match indices: rm_so=%d, rm_eo=%d\n", pmatch[1].rm_so,
        //   pmatch[1].rm_eo);
        fprintf(stderr, "Captured group 1 is invalid\n");
        free(slice);
        return NULL;
    }

    // Save matched field
    size_t match_len = (size_t) (pmatch[1].rm_eo - pmatch[1].rm_so);
    //fprintf(stderr, "Capture length: %zu\n", match_len);
    char *field = malloc(match_len + 1);
    if (!field) {
        fprintf(stderr, "Failed to allocate memory for extracted field\n");
        free(slice);
        return NULL;
    }

    memcpy(field, slice + (size_t) pmatch[1].rm_so, match_len);
    field[match_len] = '\0';

    fprintf(stderr, "Extracted field: %s\n", field);
    free(slice);
    return field;
}

void handle_bad_request(int connfd) {
    fprintf(stderr, "Bad Request\n");
    char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
    ssize_t debug = write_n_bytes(connfd, response, strlen(response));
    if (debug < 0) {
        send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                               "Content-Length: 3\r\n"
                               "Connection: close\r\n"
                               "\r\n"
                               "Bad");
    }
}
void handle_internal_error(int connfd) {
    fprintf(stderr, "Internal Server Error\n");
    char *response
        = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 21\r\n\r\nInternal Server Error\n";
    write_n_bytes(connfd, response, strlen(response));
}

void put_creation_response(int connfd) {
    char *response = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
    write_n_bytes(connfd, response, strlen(response));
}
void put_ok_response(int connfd) {
    char *response = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
    ssize_t debug = write_n_bytes(connfd, response, strlen(response));
    fprintf(stderr, "Response sent to client:\n%s\n", response);
    fprintf(stderr, "Debug write_n_bytes returned: %zd\n", debug);
}

void handle_forbidden_file(int connfd) {
    char *response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
    write_n_bytes(connfd, response, strlen(response));
}
void handle_file_not_found(int connfd) {
    //, "Here in file_not_found()\n");
    char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
    write_n_bytes(connfd, response, strlen(response));
}
void handle_unimplemented_request(int connfd) {
    char *response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n";
    write_n_bytes(connfd, response, strlen(response));
}
void handle_invalid_version(int connfd) {
    char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
    write_n_bytes(connfd, response, strlen(response));
}
void handle_unsupported_version(int connfd) {
    char *response
        = "HTTP/1.1 505 Version Not Supported\r\nContent-Length: 22\r\n\r\nVersion Not Supported\n";
    write_n_bytes(connfd, response, strlen(response));
}

// TODO :: do we need the error checks here?
char *read_until_rnrn(char *buffer, size_t *buf_len, int connfd) {
    char *rnrn = NULL;
    buffer[0] = '\0';

    while (1) {
        if (*buf_len >= MAX_HEADER_LEN) {
            // Too many header bytes without finding \r\n\r\n -> malformed
            return NULL;
        }
        size_t space = (BUFFER_SIZE - 1) - *buf_len; // keep room for '\0'
        if (space == 0) {
            // Buffer full without finding delimiter -> malformed
            return NULL;
        }
        ssize_t n = read(connfd, buffer + *buf_len, space);
        if (n == 0) {
            break;
        } else if (n < 0) {
            send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Length: 3\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Bad");
            if (errno == EINTR) {
                continue; // interrupted, retry
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket receive timed out (5s timeout from ls_accept)
                // timeout
                return NULL;
            }
            return NULL;
        }

        *buf_len += (size_t) n;
        buffer[*buf_len] = '\0';

        char *rnrn = strstr(buffer, "\r\n\r\n");
        if (rnrn != NULL) {
            return rnrn;
        }
    }

    // read into buffer until "\r\n\r\n" is found
    // We can assume this will be within 2048 bytes, so we don't need to worry about buffer overflow for this part
    while (!rnrn) {
        ssize_t bytes_read = read(connfd, (buffer + *buf_len), (BUFFER_SIZE - 1) - *buf_len);
        if (bytes_read == 0) {
            break;
        } else if (bytes_read < 0) {
            send_and_close(connfd, "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Length: 3\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "Bad");
            if (errno == EINTR) {
                continue; // interrupted, retry
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket receive timed out (5s timeout from ls_accept)
                // timeout
                return NULL;
            }
            return NULL;
        }

        // update num of bytes in buffer
        *buf_len += bytes_read;

        // buffer needs \0 to prevent running off the end
        buffer[*buf_len] = '\0';
        rnrn = strstr(buffer, "\r\n\r\n");
    }
    if (!rnrn) {
        fprintf(stderr, "Did not find \\r\\n\\r\\n in buffer\n");
        return NULL;
    }
    return rnrn;
}

void listener_init(Listener_Socket_t **sock, int port) {
    *sock = ls_new(port);

    // this is null for sudo_ports 1 - 1022
    if (*sock == NULL) {
        fprintf(stderr, "Invalid Port\n");
        exit(EXIT_FAILURE);
    }
}

off_t file_size(int fd) {
    struct stat st;
    if (fstat(fd, &st) == -1)
        return -1; // error
    return st.st_size;
}
