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

const char *delims = "\n";

void invalid_command(void);
void handle_get(char* token);
void handle_set(char* token);

int main(void) {
	// Establish our buffer
	char buffer[BUFFER_SIZE];

	// will hold text file path
	//char path[PATH_MAX];
	
	// Designate characters to use for delimiter in command (for strtokk)

	// Fill the buffer with command
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
	char *first_token = strtok(buffer, delims);
	//printf("command token read: %s\n", first_token);
		
	// Ensure command is either get or set
	if (strcmp(first_token, "get") == 0) {
    		cmd = CMD_GET;
	} else if (strcmp(first_token, "set") == 0) {
    		cmd = CMD_SET;
	} else {
		invalid_command();
	}
		
	// Grab second token of command: file location/name
	char *second_token = strtok(NULL, delims);
	//printf("file_location token read: %s\n", second_token);

	// Ensure the file name is valid
	if (strlen(second_token) >= PATH_MAX) { invalid_command(); }

	switch (cmd) {
	case CMD_GET:  handle_get(second_token); break;
	case CMD_SET:  handle_set(second_token); break;
	default:       invalid_command();
	}


	return 0;
}


void handle_get(char* token){
	// ensure there are no extra tokens in given command	
	char *extra_tokens = strtok(NULL, delims);
	//printf("Extra token: %s\n", extra_tokens ? extra_tokens : "(none)");
	if (extra_tokens) { invalid_command(); }

	int read_fd = open(token, O_RDONLY, 0);
	//printf("Open()'s output: %i\n", read_fd);
	if (read_fd < 0) { invalid_command(); }
	//printf("Proper Command!\n");
	
	// Loop until we've read and written all of the text
	while (1) {
		return;
	}
}

void handle_set(char* token){
	printf("%s", token);
	return;
}

void invalid_command(void) {
	fprintf(stderr, "Invalid Command\n");
	exit(1);
}
