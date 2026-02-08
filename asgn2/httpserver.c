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
#define REQUEST_LINE_REGEX  "(" TYPE_REGEX ") (" URI_REGEX ") (" HTTP_REGEX ")\r\n"
#define KEY_VALUE_REGEX "(" HEADER_FIELD_REGEX "):(" HEADER_VALUE_REGEX ")\r\n"
#define KEY_REGEX "^(" HEADER_FIELD_REGEX ":)"
#define VALUE_REGEX "(" HEADER_VALUE_REGEX ")\r\n"

bool g_debug = false;

#define BUFFER_SIZE 4096
char buffer[BUFFER_SIZE];

/*----- HELPERS -----*/
void listener_init(Listener_Socket_t **sock, int port);
char* read_until_rnrn(size_t *buf_len, int connfd);

char* extract_request_field(char* target_pattern, size_t offset, size_t buf_len);

void handle_get_request(char* uri, int connfd);
void handle_put_request(char* uri, int connfd, size_t *buf_len, char **rnrn_pos, size_t request_line_len);

void handle_unimplemented_request(int connfd);
void handle_invalid_version(int connfd);
void handle_unsupported_version(int connfd);
void handle_file_not_found(int connfd);
void handle_forbidden_file(int connfd);

void handle_bad_request(int connfd);

void put_ok_response(int connfd);
void put_creation_response(int connfd);
long get_content_length(char* header_field_list);


off_t file_size(int fd);


/** @brief Handles a connection from a client.
 *
 *  @param connfd The file descriptor for the connection.
 *
 *  @return void
 */
void handle_connection(int connfd) {

    size_t buf_len = 0;     // number of total bytes currently in buffer
    // size_t buf_pos = 0;     // marks the index where unused bytes begin

    // read until buffer contains \r\n\r\n ;  buffer may include subsequent payload
    char *rnrn_pos = read_until_rnrn(&buf_len, connfd);
    // TODO :: Do we need to handle instances of no \r\n\r\n ?
    // if so, then here or from within read_until_rnrn() ?
    if(!rnrn_pos) { handle_bad_request(connfd); return; }

    // print entire buffer for debugging
    fprintf(stderr, "Buffer contents after reading until \\r\\n\\r\\n:\n");
    for (size_t i = 0; i < buf_len; i++) {
        fprintf(stderr, "%c", buffer[i]);
    }

    // Use regular expressions to parse request line
    char *request_type = extract_request_field("("TYPE_REGEX")", 0, buf_len);
    buffer[strlen(request_type)] = '\0';   // null-terminate buffer at end of request type
    //fprintf(stderr, "Request type: %s\n", request_type);
    char *uri_tmp = extract_request_field( "("URI_REGEX")", strlen(request_type)+1, buf_len);   // +1 to skip \0 after request type
    //fprintf(stderr, "URI with preceding slash: %s\n", uri_tmp);
    buffer[strlen(request_type) + strlen(uri_tmp) + 1] = '\0';   // null-terminate buffer at end of URI
    char *uri = uri_tmp + 1;   // skip preceding '/'
    //fprintf(stderr, "URI: %s\n", uri);
    char *http_version = extract_request_field( "("HTTP_REGEX")\r\n" , strlen(request_type)+strlen(uri_tmp)+2, buf_len);
    //fprintf(stderr, "HTTP version: %s\n", http_version);
    fprintf(stderr, "Request type: %s\nURI: %s\nHTTP Version: %s\n", request_type, uri, http_version);


    if (!http_version) { handle_invalid_version(connfd); }

    else if (strcmp(http_version, HTTP_VERSION) != 0)   { handle_unsupported_version(connfd); }
    else if (strcasecmp(request_type, "GET") == 0)      { handle_get_request(uri, connfd); }
    else if (strcasecmp(request_type, "PUT") == 0)      {
        size_t request_line_len = strlen(request_type) + strlen(uri_tmp) + strlen(http_version) + 4;   // +4 for spaces and \r\n
        handle_put_request(uri, connfd, &buf_len, &rnrn_pos, request_line_len);
    }   

    else { handle_unimplemented_request(connfd); }
    
    
    //printf("buffer unread starts at: %c %c %c %c\n", buffer[i-1], buf_pos[i], buf_pos[i+1], buf_pos[i+2]);
    //printf("last few bytes: %c %c %c \n", buffer[bytes_read-3], buffer[bytes_read-2], buffer[bytes_read-1]);

    fprintf(stderr, "Free!\n");
    free(request_type);
    free(uri_tmp);
    free(http_version);
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
        close(connfd);
    }

    ls_delete(&sock);
    return EXIT_SUCCESS;
}


// TODO :: special instances of erroneous requests
void handle_get_request(char* uri, int connfd) {
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
                exit(EXIT_FAILURE);
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
    free(response);

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


void handle_put_request(char* uri, int connfd, size_t *buf_len, char **rnrn_pos, size_t request_line_len) {
    // Check if file exists but is not a regular file
    struct stat st;
    if(stat(uri, &st) == 0) {
        fprintf(stderr, "File already exists: %s\n", uri);
        // file exists
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "FILE IS FORBIDDEN: %s\n", uri);
            // file is not regular file
            handle_forbidden_file(connfd);
            return;
        }
    }

    // Create file if it doesn't exist
    int wfd = open(uri, O_WRONLY | O_TRUNC | O_CREAT | O_EXCL, 0666);
    if (wfd >= 0) {
        //Newly created file
        fprintf(stderr, "Just created: %s\n", uri);
        put_creation_response(connfd);
    }
    else if (errno == EEXIST) {
        // File already exists, open for writing
        wfd = open(uri, O_WRONLY | O_TRUNC);
        if (wfd < 0) { // TODO :: might not be correct error-check here
            fprintf(stderr, "Error opening existing file for writing\n");
            handle_file_not_found(connfd);
            return;
        }
        fprintf(stderr, "File already exists: %s\nIt was successfully opened\n", uri);
        put_ok_response(connfd);
    }
    else {
        // Other error
        fprintf(stderr, "Error opening file for writing\n");
        handle_file_not_found(connfd);
        return;
    }

    g_debug = true;


    //size_t start_of_msg = (size_t)(rnrn_pos - buffer) + 4;   // index of first byte after \r\n\r\n
    char *header_field_list = buffer + request_line_len;  // index of first byte of header fields (after request line)
    //fprintf(stderr, "\nheader_field_list: \n%s\n", header_field_list);
    if (!header_field_list) {
        fprintf(stderr, "header_field_list not found\n");
        close(wfd);
        handle_bad_request(connfd);
        return;
    }

    //for (size_t i = 0; i < 10; i++) {
    //    fprintf(stderr, "header_field_list[%zu]: %c\n", i, header_field_list[i]);
    //}

    //fprintf(stderr, "header_field_list:\n\n\n\n%s\n", header_field_list);
    size_t content_length = 0;
    //fprintf(stderr, "\n\nWE HERE WIT IT!!!!!!!!!!!!!!!\n\n");
    buffer[*buf_len] = '\0';   // null-terminate buffer for regex functions
    // look for content-length header-field key and value while validating header fields with regex
    while (1) {
        fprintf(stderr, "\nExtracting header key from: \n%s", header_field_list);
        //fprintf(stderr, "TESTING\n");
        //handle_bad_request(connfd);
        //return;

        fprintf(stderr, "Buffer contents at start of loop:\n");
        for (size_t i = 0; i < *buf_len; i++) {
            fprintf(stderr, "%c", buffer[i]);
        }
        fprintf(stderr, "\n");
        //fprintf(stderr, "Offset being used is: %zu\n", header_field_list - buffer);
        //fprintf(stderr, "first few chars at offset: %c %c %c %c\n", buffer[header_field_list - buffer], buffer[header_field_list - buffer + 1], buffer[header_field_list - buffer + 2], buffer[header_field_list - buffer + 3]);
        //fprintf(stderr, "Buffer length: %zu\n", *buf_len);

        char *header_key = extract_request_field(KEY_REGEX, header_field_list - buffer, *buf_len);
        if (!header_key) {
            fprintf(stderr, "Header Key not found\n");
            close(wfd);
            handle_bad_request(connfd);
            return;
        }
        //fprintf(stderr, "header_key: %s\n", header_key);

        fprintf(stderr, "Extracting header value...\n");
        size_t base = (size_t)(header_field_list - buffer) + strlen(header_key) + 1;
        //fprintf(stderr, "base index for value regex: %zu\n", base);
        //for (size_t idx = 0; idx < 4; idx++) {
        //    fprintf(stderr, "buffer[%zu]: %c\n", base + idx, buffer[base + idx]);
        //}
        //fprintf(stderr, "length of header key: %zu\n", strlen(header_key));
        char *header_value = extract_request_field(VALUE_REGEX, base, *buf_len);
        //fprintf(stderr, "MADE IT BACK\n");
        if (!header_value) {
            fprintf(stderr, "Header value not found for key: %s\n", header_key);
            free(header_key);
            close(wfd);
            handle_bad_request(connfd);
            return;
        }
        fprintf(stderr, "header_value: %s\n", header_value);

        fprintf(stderr, "Checking if header key is Content-Length...\n");
        if (strncmp(header_key, "Content-Length", strlen("Content-Length")) == 0) {
            char *content_length_str = header_value;
            fprintf(stderr, "content_length_str: %s\n", content_length_str);
            content_length = strtoul(content_length_str, NULL, 10);
            fprintf(stderr, "first size_t stroul conversion content_length: %zu\n", content_length);
            free(header_key);
            free(header_value);
            break; }

        else {
            header_field_list += strlen(header_key) + strlen(header_value) + 3;   // move to next header field (skip \r\n)
            free(header_key);
            free(header_value);
            if ((size_t)(header_field_list - buffer) >= *buf_len) {
                fprintf(stderr, "End of header fields not found\n");
                close(wfd);
                handle_bad_request(connfd);
                return;
            }
        }
    }

    //char *content_len_pos = strstr(header_field_list, "Content-Length: ");
    //fprintf(stderr, "content_len_pos after regex: %s\n", content_len_pos);

    size_t bytes_requested = content_length;
    fprintf(stderr, "content length conversion, bytes_requested: %zu\n", bytes_requested);

    char *payload_pos = *rnrn_pos + 4;   // index of first byte after \r\n\r\n
    //fprintf(stderr, "buf_len: %zu\n", *buf_len);
    //fprintf(stderr, "rnrn_pos - buffer: %zu\n", *rnrn_pos - buffer);
    //fprintf(stderr, "payload_pos - buffer: %zu\n", payload_pos - buffer);

    ssize_t payload_bytes_in_buffer = *buf_len - (payload_pos - buffer);
    fprintf(stderr, "payload_bytes_in_buffer: %zu\n", payload_bytes_in_buffer);
    bytes_requested -= write_n_bytes(wfd, payload_pos, payload_bytes_in_buffer);

    while (bytes_requested > 0) {
        ssize_t bytes_read = read_n_bytes(connfd, buffer, BUFFER_SIZE);
        if (bytes_read == 0) { break; }
        ssize_t bytes_to_write = (bytes_read < (ssize_t)bytes_requested) ? bytes_read : bytes_requested;
        ssize_t bytes_written = write_n_bytes(wfd, buffer, bytes_to_write);
        bytes_requested -= bytes_written;
    }

//    size_t bytes_to_write = bytes_requested - (*buf_len - (payload_pos - buffer));
 //   fprintf(stderr, "bytes to write: %ld\n", bytes_to_write);
    //fprintf(stderr, "Payload: %c\n%c\n%c\n%c\n", payload_pos[8], payload_pos[9], payload_pos[10], payload_pos[11]);
    //fprintf(stderr, "rnrn_pos: %c\n%c\n%c\n%c\n", (*rnrn_pos)[8], (*rnrn_pos)[9], (*rnrn_pos)[10], (*rnrn_pos)[11]);
   // fprintf(stderr, "Buffer_len: %ld\n", *buf_len);
    //fprintf(stderr, "buffer end: %c\n%c\n%c\n%c\n", buffer[*buf_len-8], buffer[*buf_len-7], buffer[*buf_len-6], buffer[*buf_len-5]);

    //for (size_t i = 0; i < *buf_len; i++) {
    //    fprintf(stderr, "buffer[%zu]: %c\n", i, buffer[i]);
    //}

    close(wfd);
}


char* extract_request_field(char *target_pattern, size_t offset, size_t buf_len) {
    if (offset > buf_len) { fprintf(stderr, "Offset exceeds buffer length\n"); return NULL; }

    if (g_debug) {
        fprintf(stderr, "Extracting field with pattern: %s\nfrom buffer starting at offset: %zu\n", target_pattern, offset);
        //for (size_t i = 0; i < 5; i++) {
        //    fprintf(stderr, "buffer[%zu]: %c\n", offset + i, buffer[offset + i]);
        //}
    }

    //fprintf(stderr, "Extracting field with pattern: %s\nfrom buffer starting at offset: %zu\n", target_pattern, offset);
    //    for (size_t i = 0; i < 5; i++) {
    //    fprintf(stderr, "buffer[%zu]: %c\n", offset + i, buffer[offset + i]);
    //}

    // Compile regular expression
    regex_t regex;
    int rc = regcomp(&regex, target_pattern, REG_EXTENDED);
    if (rc != 0) { fprintf(stderr, "Regex compilation failed with code: %d\n", rc); return NULL; }

    // pmatch holds indices of matched group
    regmatch_t pmatch[2];

   //if (g_debug) {
   //    fprintf(stderr, "About to regexec starting from offset: %zu\n", offset);
   //    fprintf(stderr, "About to regexec starting from offset: %c\n", buffer[offset-1]);
   //}
    // execute regex search
    int match = regexec(&regex, &buffer[offset], 2, pmatch, 0);
    regfree(&regex);	// free memory used for regex compilation

    if (match != 0) { return NULL; }

    // Save matched field
    size_t match_len = pmatch[1].rm_eo - pmatch[1].rm_so;
    fprintf(stderr, "Capture length: %zu\n", match_len);
    char *field = malloc(match_len + 1);
    memcpy(field, buffer + offset + pmatch[1].rm_so, match_len);
    field[match_len] = '\0';

    fprintf(stderr, "Extracted field: %s\n", field);
    return field;
}

void handle_bad_request(int connfd) {
    fprintf(stderr, "Bad Request\n");
    char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
    write_n_bytes(connfd, response, strlen(response));
}

void put_creation_response(int connfd) {
    char *response = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
    write_n_bytes(connfd, response, strlen(response));
}
void put_ok_response(int connfd) {
    char *response = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
    write_n_bytes(connfd, response, strlen(response));
}


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



// TODO :: do we need the error checks here?
char* read_until_rnrn(size_t *buf_len, int connfd){
    char* rnrn = NULL;

    // read into buffer until "\r\n\r\n" is found
    // We can assume this will be within 2048 bytes, so we don't need to worry about buffer overflow for this part
    while (!rnrn) {
    	ssize_t bytes_read = read(connfd, (buffer + *buf_len), (BUFFER_SIZE - 1) - *buf_len);
    	if(bytes_read == 0) { break; }
        else if (bytes_read < 0) { fprintf(stderr, "Error reading from socket\n"); return NULL; }

        // update num of bytes in buffer
    	*buf_len += bytes_read;

        // buffer needs \0 to prevent running off the end
        buffer[*buf_len] = '\0';
    	rnrn = strstr(buffer, "\r\n\r\n");
    }
    if (!rnrn) { fprintf(stderr, "Did not find \\r\\n\\r\\n in buffer\n"); return NULL; }
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
