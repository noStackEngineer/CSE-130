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

// potentially ^ to beginning
#define REQUEST_LINE_REGEX  "(" TYPE_REGEX ") (" URI_REGEX ") (" HTTP_REGEX ")\r\n"

#define BUFFER_SIZE 4096
char buffer[BUFFER_SIZE];

/*----- HELPERS -----*/
void listener_init(Listener_Socket_t **sock, int port);
char* read_until_rnrn(size_t *buf_len, int connfd);

char* extract_request_field(char* target_pattern);
void handle_get_request(char* uri, int connfd);

void handle_unimplemented_request(int connfd);
void handle_invalid_version(int connfd);
void handle_unsupported_version(int connfd);
void handle_file_not_found(int connfd);
void handle_forbidden_file(int connfd);

off_t file_size(int fd);


//void extract_request(char **req_type, char **uri, char **version);
//void handle_put_request(char **unprocessed_pos);
//void handle_bad_request(int connfd);
//char* build_response(int status_code, char* status_txt, long long content_len);//, size_t &header_len);

/** @brief Handles a connection from a client.
 *
 *  @param connfd The file descriptor for the connection.
 *
 *  @return void
 */
void handle_connection(int connfd) {

    //printf("%s\n", TYPE_REGEX);
    //printf("%s\n", URI_REGEX);
    //printf("%s\n", HTTP_REGEX);
    ////char *buf_pos = buffer;
    size_t buf_len = 0;     // number of total bytes currently in buffer
    //size_t buf_pos = 0;     // marks the index where unused bytes begin

    // read until buffer contains \r\n\r\n ;  buffer may include subsequent payload
    char *rnrn_pos = read_until_rnrn(&buf_len, connfd);
    // TODO :: Do we need to handle instances of no \r\n\r\n ?
    // if so, then here or from within read_until_rnrn() ?
    if(!rnrn_pos) { printf("hell nah"); }

    
    // Use regular expressions to parse request line
    char *request_type = extract_request_field( "("TYPE_REGEX")" );
    char *uri_tmp = extract_request_field( "("URI_REGEX")" );
    char *uri = uri_tmp + 1;   // skip preceding '/'
    char *http_version = extract_request_field( "("HTTP_REGEX")\r\n" );
    // printf("Request type: %s\nURI: %s\nHTTP Version: %s\n", request_type, uri, http_version);


    if (!http_version) { handle_invalid_version(connfd); }

    else if (strcmp(http_version, HTTP_VERSION) != 0)   { handle_unsupported_version(connfd); }
    else if (strcasecmp(request_type, "GET") == 0)      { handle_get_request(uri, connfd); }

    else { handle_unimplemented_request(connfd); }
    
    
    //printf("buffer unread starts at: %c %c %c %c\n", buffer[i-1], buf_pos[i], buf_pos[i+1], buf_pos[i+2]);
    //printf("last few bytes: %c %c %c \n", buffer[bytes_read-3], buffer[bytes_read-2], buffer[bytes_read-1]);

    fprintf(stderr, "Free!\n");
    free(request_type);
    free(uri_tmp);
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

/*
char* build_response(int status_code, char* status_txt, long long content_len){//, size_t &header_len) {
	//int status_
	int needed = snprintf(NULL, 0,
            HTTP_VERSION " %d %s\r\n"
            "Content-Length: %lld\r\n\r\n",
            status_code,
            status_txt,
	    content_len
        );

	// *header_len = needed;
	//needed += content_len;
        
        char *response = malloc(needed + 1);
        snprintf(response, needed + 1,
            HTTP_VERSION " %d %s\r\n"
            "Content-Length: %lld\r\n\r\n",
            status_code,
            status_txt,
	    content_len
        );
	response[needed] = '\0';
	
	return response;
}
*/

void handle_get_request(char* uri, int connfd) {
    // Attempt to open the requested file
    int rfd = open(uri, O_RDONLY);
    //fprintf(stderr, "URI: %s\n", uri);

    // If open failed, respond to client with appropriate msg
    if (rfd < 0) {
        switch (errno) {
            case ENOENT:
                // TODO :: do i keep this? do we close rfd here?
            	// file does not exist
                close(rfd);
                handle_file_not_found(connfd);
                return;
            case EACCES:
                // TODO :: do i keep this? do we close rfd here?
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
                exit(EXIT_FAILURE);
            	break;
    	}
    }
    //fprintf(stderr, "RFD: %d\n", rfd);

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
            fsize
        );
    response = malloc(response_len + 1);
    snprintf(response, response_len + 1,
            HTTP_VERSION " 200 OK\r\n"
            "Content-Length: %ld\r\n\r\n",
            fsize
        );
    response[response_len] = '\0';

    // Send response header to client
    write_n_bytes(connfd, response, response_len);

    // Call read and write until EOF
    while(1) {
        ssize_t bytes_read = read_n_bytes(rfd, buffer, BUFFER_SIZE);
        ssize_t bytes_written = write_n_bytes(connfd, buffer, bytes_read);
        if (bytes_read == 0) { break; }
        else if (bytes_read < 0) { fprintf(stderr, "Error reading file\n"); exit(EXIT_FAILURE); }
        else if (bytes_written < 0) { fprintf(stderr, "Error writing file\n"); exit(EXIT_FAILURE); }
    }
    
    close(rfd);
    return;
}

/*
void handle_put_request(char **unprocessed_pos) {
	//printf("We gotta put request ova hea\n");
	if (!unprocessed_pos) {exit(1);}
	return;
}

void handle_bad_request(void) {
	//printf("We gotta bad request ova hea\n");
	return;
}
*/

void handle_forbidden_file(int connfd){
    char *response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
    write_n_bytes(connfd, response, strlen(response));
}
void handle_file_not_found(int connfd){
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
    char *response = "HTTP/1.1 505 Version Not Supported\r\nContent-Length: 22\r\n\r\nVersion Not Supported\n";
    write_n_bytes(connfd, response, strlen(response));
}

// TODO :: Clean up function and add comment header/desc.
// TODO :: Actually, we're probably getting rid of this one
/*
void extract_request(char **req_type, char **uri, char **version){ 
    // Compile regular expression
    regex_t regex;
    char* pat = REQUEST_LINE_REGEX;     //  "(" TYPE_REGEX ") (" URI_REGEX ") (" HTTP_REGEX ")\r\n"
    regcomp(&regex, pat, REG_EXTENDED);

    // pmatch holds indices of matched groups
    regmatch_t pmatch[4];

    // execute regex search
    int match = regexec(&regex, buffer, 4, pmatch, 0);
    regfree(&regex);	// free memory used for regex compilation

    // TODO :: might not need this, can we assume always matches?
    if (match != 0) { fprintf(stderr, "NO MATCH\n"); exit(EXIT_FAILURE); }
    // Save request_type
    size_t req_type_len = pmatch[1].rm_eo - pmatch[1].rm_so;
    *req_type = malloc(req_type_len + 1);
    memcpy(*req_type, buffer + pmatch[1].rm_so, req_type_len);
    (*req_type)[req_type_len] = '\0';
    //snprintf(req_type, req_type_len, "%s", buffer+pmatch[1].rm_so);

    // Save uri - excluding preceding '/'
    size_t uri_len = pmatch[2].rm_eo - pmatch[2].rm_so - 1;
    *uri = malloc(uri_len + 1);
    memcpy(*uri, buffer + pmatch[2].rm_so + 1, uri_len);
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
}*/

char* extract_request_field(char *target_pattern){
    // Compile regular expression
    regex_t regex;
    regcomp(&regex, target_pattern, REG_EXTENDED);

    // pmatch holds indices of matched group
    regmatch_t pmatch[2];

    // execute regex search
    int match = regexec(&regex, buffer, 2, pmatch, 0);
    regfree(&regex);	// free memory used for regex compilation

    if (match != 0) { return NULL; }

    // Save matched field
    size_t match_len = pmatch[1].rm_eo - pmatch[1].rm_so;
    char *field = malloc(match_len + 1);
    memcpy(field, buffer + pmatch[1].rm_so, match_len);
    field[match_len] = '\0';

    return field;
}

// TODO :: do we need the error checks here?
char* read_until_rnrn(size_t *buf_len, int connfd){
    char* rnrn = NULL;

    // read into buffer until "\r\n\r\n" is found
    while (!rnrn) {
	    //if (*bytes_read == request_limit) { fprintf(stderr, "NO rnrn found\n"); exit(EXIT_FAILURE); }
        // TODO :: third arg can become negative
    	ssize_t bytes_read = read(connfd, (buffer + *buf_len), (BUFFER_SIZE - *buf_len -1));
    	if(bytes_read == 0) { break; }
    	//else if (bytes_read < 0) { printf("HEREfrfrfr\n"); }

        // update num of bytes in buffer
    	*buf_len += bytes_read;

        // buffer needs \0 to prevent running off the end
        buffer[*buf_len] = '\0';
    	rnrn = strstr(buffer, "\r\n\r\n");
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
        return -1;   // error
    return st.st_size;
}
