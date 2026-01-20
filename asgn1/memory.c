#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096

typedef enum {
    CMD_NONE,
    CMD_GET,
    CMD_SET
} command_t;
command_t cmd = CMD_NONE;

// Designate characters to use for delimiter in command (for strtok)
const char *delims = "\n";

void invalid_command(void);
void handle_get(char* location_token, char* buffer);
void handle_set(char* location_token, char* buffer);

int main(void) {
	char buffer[BUFFER_SIZE];

	// Fill buffer with stdin stream
	ssize_t bytes_read = read(0, buffer, BUFFER_SIZE);

	// Check for error or empty read
	if (bytes_read < 0) {
		fprintf(stderr, "call to read() returned error\n");
		exit(1);
	}
	if (bytes_read == 0) {
		printf("no bytes to read\n");
	}

	//printf("%zi bytes were successfully read!\n", bytes_read);
		
	// Grab first token of command
	char *cmd_token = strtok(buffer, delims);
	//printf("command token read: %s\n", cmd_token);
		
	// Ensure command is either get or set
	if (strcmp(cmd_token, "get") == 0) {
    		cmd = CMD_GET;
	} else if (strcmp(cmd_token, "set") == 0) {
    		cmd = CMD_SET;
	} else {
		invalid_command();
	}
		
	// Grab second token of command: file location/name
	char *location_token = strtok(NULL, delims);
	
	//printf("location token read: %s\n", location_token);
	//printf("end of token read: %c\n", buffer[bytes_read-1]);
	if (!location_token) { invalid_command(); }
	
	// Ensure the file name is valid
	if (strlen(location_token) >= PATH_MAX) { invalid_command(); }

	//printf("HERE\n");

	switch (cmd) {
	case CMD_GET:  handle_get(location_token, buffer); break;
	case CMD_SET:  handle_set(location_token, buffer); break;
	default:       invalid_command();
	}


	return 0;
}


void handle_get(char* location_token, char* buffer){
	// ensure there are no extra tokens in given command	
	char *extra_tokens = strtok(NULL, delims);
	//printf("Extra token: %s\n", extra_tokens ? extra_tokens : "(none)");
	if (extra_tokens) { invalid_command(); }

	// open file in read only mode
	int read_fd = open(location_token, O_RDONLY);
	//printf("Open()'s output: %i\n", read_fd);
	// If file doesn't exit, exit
	if (read_fd < 0) { invalid_command(); }
	//printf("Proper Command!\n");
	
	// Loop until we've read and written all of the text
	while (1) {
		ssize_t bytes_read = read(read_fd, buffer, BUFFER_SIZE);
		
		// Check for error or empty read
		if (bytes_read < 0) {
			fprintf(stderr, "call to read() returned error\n");
			exit(1);
		}
		if (bytes_read == 0) {
			//printf("no bytes to read\n");
			break;
		}

		char *p = buffer;
		ssize_t bytes_to_write = bytes_read;

		while(bytes_to_write > 0) {
			ssize_t bytes_written = write(1, p, bytes_to_write);

			if(bytes_written < 0) {
				close(read_fd);
				fprintf(stderr, "call to write() returned error\n");
				exit(1);
			}

			p += bytes_written;
			bytes_to_write -= bytes_written;
		}
	}
}

void handle_set(char* location_token, char* buffer){
	char* length_token = strtok(NULL, delims);
	if (!length_token) { printf("no length"); invalid_command(); }

	char* content_token = strtok(NULL, delims);
	if (!content_token) { printf("no content"); invalid_command(); }
	
	int write_fd = open(location_token, O_WRONLY|O_CREAT|O_TRUNC);
	if (write_fd < 0) { invalid_command(); }

	while(1) {
		if (buffer) { printf("Buffer[0]: %c", buffer[0]); }
		printf("good so far!\n");
		exit(0);
	}
}

void invalid_command(void) {
	fprintf(stderr, "Invalid Command\n");
	exit(1);
}


















