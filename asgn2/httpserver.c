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

#define REQUEST_LINE_REGEX  "(" TYPE_REGEX ") (" URI_REGEX ") (" HTTP_REGEX ")"

#define BUFFER_SIZE 4096
char buffer[BUFFER_SIZE];

/*----- HELPERS -----*/
void listener_init(Listener_Socket_t **sock, int port);
char* read_until_rnrn(char **buf_pos, ssize_t *bytes_read, int connfd);
void extract_request(char **req_type, char **uri, char **version);

/** @brief Handles a connection from a client.
 *
 *  @param connfd The file descriptor for the connection.
 *
 *  @return void
 */
void handle_connection(int connfd) {

    char* buf_pos = buffer;
    ssize_t bytes_read = 0;

    char* rnrn_pos = read_until_rnrn(&buf_pos, &bytes_read, connfd);

    if(!rnrn_pos) { printf("hell nah"); }
    // TODO :: Do we need to handle instances of no \r\n\r\n ?
    // it's currently handled in read_until_rnrn() but maybe we remove it

    // Use regular expressions to parse request line
    char *request_type = NULL;
    char *uri = NULL;
    char *http_version = NULL;
    extract_request(&request_type, &uri, &http_version);

    //printf("Request type: %s\nURI: %s\nHTTP Version: %s\n", request_type, uri, http_version);


    free(request_type);
    free(uri);
    free(http_version);
    close(connfd);
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
        handle_connection(connfd);
    }

    ls_delete(&sock);
    return EXIT_SUCCESS;
}

char* read_until_rnrn(char **buf_pos, ssize_t *bytes_read, int connfd){
    ssize_t request_limit = 2048;

    char* rnrn = NULL;

    while (!rnrn) {
	if (*bytes_read == request_limit) { fprintf(stderr, "NO rnrn found\n"); exit(EXIT_FAILURE); }
    	*bytes_read += read_n_bytes(connfd, *buf_pos, request_limit - *bytes_read);
    	// TODO :: how many bytes do we want to read here?
    	buffer[*bytes_read] = '\0';
	rnrn = strstr(*buf_pos, "\r\n\r\n");
	buf_pos += *bytes_read;
    }
    return rnrn;
}

void extract_request(char **req_type, char **uri, char **version){ 
    // Compile regular expression
    regex_t regex;
    char* pat = REQUEST_LINE_REGEX;
    regcomp(&regex, pat, REG_EXTENDED);

    // pmatch holds indices of matched groups
    regmatch_t pmatch[4];

    // execute regex search
    int match = regexec(&regex, buffer, 4, pmatch, 0);
    regfree(&regex);	// free memory used for regex compilation

    // TODO :: might not need this, maybe we can assume always matches?
    if (match != 0) { fprintf(stderr, "NO MATCH\n"); exit(EXIT_FAILURE); }

    // Save request_type
    size_t req_type_len = pmatch[1].rm_eo - pmatch[1].rm_so;
    *req_type = malloc(req_type_len + 1);
    memcpy(*req_type, buffer + pmatch[1].rm_so, req_type_len);
    (*req_type)[req_type_len] = '\0';
    //snprintf(req_type, req_type_len, "%s", buffer+pmatch[1].rm_so);

    // Save uri
    size_t uri_len = pmatch[2].rm_eo - pmatch[2].rm_so;
    *uri = malloc(uri_len + 1);
    memcpy(*uri, buffer + pmatch[2].rm_so, uri_len);
    (*uri)[uri_len] = '\0';
    //snprintf(uri, uri_len, "%s", buffer+pmatch[2].rm_so);

    // Save http version
    size_t version_len = pmatch[3].rm_eo - pmatch[3].rm_so;
    *version = malloc(version_len + 1);
    memcpy(*version, buffer + pmatch[3].rm_so, version_len);
    (*version)[version_len] = '\0';
    //snprintf(version, version_len, "%s", buffer+pmatch[3].rm_so);
    
    // Save match in buffer directly
    //char* cap2 = buffer + pmatch[1].rm_so;
    //buffer[pmatch[1].rm_eo] = '\0';
}

void listener_init(Listener_Socket_t **sock, int port) {
    *sock = ls_new(port);
    
    // this is null for sudo_ports 1 - 1022
    if (*sock == NULL) {
	    fprintf(stderr, "Invalid Port\n");
	    exit(EXIT_FAILURE);
    }
}

