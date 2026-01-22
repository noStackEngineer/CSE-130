#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdbool.h>

#define BUFFER_SIZE PATH_MAX

typedef enum { CMD_NONE, CMD_GET, CMD_SET } command_t;
command_t cmd = CMD_NONE;

// Designate characters to use for delimiter in command (for strtok)
const char *delims = "\n";

void invalid_command(void);
void handle_get(char *filename, char *buffer);
void handle_set(char *filename, char *buffer, size_t bytes_in_buffer);

int main(void) {
    char buffer[BUFFER_SIZE];

    // Fill buffer with command in stdin stream
    size_t bytes_in_buffer = 0;

    // loop until we have our command ["get\n", "set\n"]
    while (bytes_in_buffer < 4) {
        ssize_t r = read(STDIN_FILENO, buffer + bytes_in_buffer, 4 - bytes_in_buffer);

        // Check for error or empty read
        if (r < 0) {
            fprintf(stderr, "call to read() returned error\n");
            exit(1);
        }
        if (r == 0) {
            invalid_command();
        }

        bytes_in_buffer += (size_t) r;
    }
    //printf("%zi bytes were successfully read!\n", bytes_read);

    // Ensure command is either get or set
    if (memcmp(buffer, "get\n", 4) == 0) {
        cmd = CMD_GET;
    } else if (memcmp(buffer, "set\n", 4) == 0) {
        cmd = CMD_SET;
    } else {
        invalid_command();
    }

    // Grab second token of command: file location/name
    bytes_in_buffer = 0;
    char filename[PATH_MAX];

    while (1) {
        if (bytes_in_buffer >= (size_t) BUFFER_SIZE) {
            invalid_command();
        }

        char *pos = buffer + bytes_in_buffer;
        size_t buffer_space_left = (size_t) BUFFER_SIZE - 1 - bytes_in_buffer;

        ssize_t r = read(STDIN_FILENO, pos, buffer_space_left);

        // Check for error or empty read
        if (r < 0) {
            fprintf(stderr, "call to read() returned error\n");
            exit(1);
        }
        if (r == 0) {
            invalid_command(); //TODO :: EOF of stdin, invalid command() ?
                // I believe so because it would mean stdin
                // is closed and newline hasn't been found
            //printf("no bytes to read\n");
            //break;
        }
        bytes_in_buffer += (size_t) r;

        // check if newline in what was just read
        char *newline = memchr(pos, '\n', (size_t) r);

        if (newline) {
            size_t filename_length = newline - buffer;
            if (filename_length == 0 || filename_length >= PATH_MAX) {
                invalid_command();
            }

            memcpy(filename, buffer, filename_length);
            filename[filename_length] = '\0';

            // ensure get command doesn't have extra input
            if (cmd == CMD_GET) {
                size_t consumed = filename_length + 1;
                if (bytes_in_buffer > consumed) {
                    invalid_command();
                }

                char garbage;
                ssize_t extra = read(STDIN_FILENO, &garbage, 1);
                if (extra < 0) {
                    fprintf(stderr, "call to read() returned error\n");
                    exit(1);
                }
                if (extra > 0) {
                    invalid_command();
                }
            }
            break;
        }
    }

    switch (cmd) {
    case CMD_GET: handle_get(filename, buffer); break;
    case CMD_SET: handle_set(filename, buffer, bytes_in_buffer); break;
    default: invalid_command();
    }

    return 0;
}

void handle_get(char *filename, char *buffer) {
    // open file in read only mode
    int read_fd = open(filename, O_RDONLY);

    // If file doesn't exist, exit
    if (read_fd < 0) {
        invalid_command();
    }

    // Loop until we've read and written all of the text
    while (1) {
        char *pos = buffer;
        ssize_t bytes_read = read(read_fd, buffer, BUFFER_SIZE);

        // Check for error or empty read
        if (bytes_read < 0) {
            invalid_command();
        }
        if (bytes_read == 0) {
            break; // EOF
        }

        ssize_t bytes_to_write = bytes_read;

        while (bytes_to_write > 0) {
            ssize_t bytes_written = write(1, pos, bytes_to_write);

            if (bytes_written < 0) {
                close(read_fd);
                fprintf(stderr, "call to write() returned error\n");
                exit(1);
            }

            pos += bytes_written;
            bytes_to_write -= bytes_written;
        }
    }
    close(read_fd);
}

// <filename>\n<content_length>\n<contents>
void handle_set(char *filename, char *buffer, size_t bytes_in_buffer) {
    char digit_string[32]; // user specifies # of bytes to write
    size_t num_of_digits = 0; // strlen won't work on it, so track its length
    size_t bytes_requested = 0; // final numeric conversion from digit_string

    // check for unprocessed buffer bytes, process if present
    size_t end_of_filename = strlen(filename) + 1; // file.txt + \n
    if (bytes_in_buffer < end_of_filename) {
        invalid_command();
    }

    ssize_t unprocessed_bytes = bytes_in_buffer - end_of_filename;

    /*    printf("Filename: '%s'\n", filename);
    printf("length: '%lu'\n", strlen(filename));
    printf("end_of_filename: '%lu'\n", end_of_filename);
    printf("Unprocessed Bytes: %lu\n", unprocessed_bytes);
    printf("Bytes_in_buffer: %lu\n", bytes_in_buffer);	*/
    char *pos = buffer + end_of_filename;

    /*
    char* test = pos;
    for (int i = -2; i <= 2; i++) {
        char *p = test + i;

        if (p < buffer || p >= buffer + bytes_in_buffer) {
    	    printf("[%2d]: (out of bounds)\n", i);
        } else if (*p == '\n') {
	    printf("[%2d]: '\\n'\n", i);
        } else if (*p == '\r') {
	    printf("[%2d]: '\\r'\n", i);
        } else {
	    printf("[%2d]: '%c' (0x%02x)\n", i, *p, (unsigned char)*p);
        }
    }*/

    while (unprocessed_bytes > 0) {
        // memchr to look for newline
        char *newline = memchr(pos, '\n', unprocessed_bytes);

        // if found, memcpy into byte_request[] and break
        if (newline) {
            num_of_digits = newline - pos;
            if (num_of_digits >= 32) {
                invalid_command();
            }
            //		    printf("number of digits: %lu\n", num_of_digits);

            memcpy(digit_string, pos, num_of_digits);
            pos = newline + 1;
            unprocessed_bytes = bytes_in_buffer - (pos - buffer);
            /*  printf("HERE\n");
		    printf("num_of_digits: %lu\n", num_of_digits);
		    printf("digit_string: %s\n", digit_string);
		    printf("unproc bytes: %lu\n", unprocessed_bytes);
		    */
            break;
        }

        // else, memcpy into byte_request and call read
        if (unprocessed_bytes >= 32) {
            invalid_command();
        }

        // save first portion of number
        memcpy(digit_string, pos, unprocessed_bytes);
        num_of_digits += unprocessed_bytes;

        //printf("HERE\n");
        // proceed with reading until "<content_length>\n" is parsed
        while (1) {
            ssize_t r = read(STDIN_FILENO, buffer, BUFFER_SIZE);

            // check for error or empty read
            if (r <= 0) {
                invalid_command();
            }

            pos = buffer;
            char *newline = memchr(pos, '\n', r);

            if (newline) {
                size_t new_digits = newline - pos;
                if (num_of_digits + new_digits >= 32) {
                    invalid_command();
                }

                memcpy(digit_string + num_of_digits, pos, new_digits);
                num_of_digits += new_digits;
                pos = newline + 1;
                bytes_in_buffer = r;
                unprocessed_bytes = (size_t) r - (size_t) (pos - buffer);
                break;
            } else {
                if (num_of_digits + r >= 32) {
                    invalid_command();
                }
                memcpy(digit_string + num_of_digits, pos, r);
                num_of_digits += r;
            }
        }
        break;
    }
    //printf("HERE\n");

    // attempt to convert digit_string to actual number
    digit_string[num_of_digits] = '\0';
    char *end;
    long num_tmp = strtol(digit_string, &end, 10);

    // ensure valid number was found
    if (errno != 0 || *end != '\0') {
        invalid_command();
    }

    if (num_tmp < 0) { // || num_tmp > (10 * 1024 * 1024)) {
        invalid_command();
    }

    //printf("HERE\n");
    bytes_requested = (size_t) num_tmp;
    //printf("Number - 1 = %d\n", content_length - 1);

    // Open file to begin writing to it
    int write_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (write_fd < 0) {
        invalid_command();
    }

    //printf("length extracted: %zu\n", bytes_requested);

    ssize_t bytes_written = 0;
    ssize_t requested_bytes_remaining = bytes_requested - bytes_written;
    ssize_t bytes_to_write = 0;

    // let's process anything already in buffer first
    while (unprocessed_bytes > 0 && requested_bytes_remaining > 0) {
        bytes_to_write = (requested_bytes_remaining > unprocessed_bytes)
                             ? unprocessed_bytes
                             : requested_bytes_remaining;

        bytes_written = write(write_fd, pos, bytes_to_write);

        if (bytes_written < 0) {
            fprintf(stderr, "Operation Failed");
            close(write_fd);
            exit(1);
        } else if (bytes_written == 0) {
            // dont think this should happen
            break;
        }

        pos += bytes_written;
        requested_bytes_remaining -= bytes_written;
        unprocessed_bytes -= bytes_written;
    }

    // Finally, we read and write until done
    while (1) {
        if (requested_bytes_remaining <= 0) {
            break;
        }

        ssize_t bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE);

        // check for error or empty read
        if (bytes_read < 0) {
            invalid_command();
        }
        if (bytes_read == 0) {
            break;
        }

        pos = buffer;
        bytes_to_write
            = (requested_bytes_remaining > bytes_read) ? bytes_read : requested_bytes_remaining;

        while (bytes_to_write > 0 && requested_bytes_remaining > 0) {
            bytes_written = write(write_fd, pos, bytes_to_write);

            if (bytes_written < 0) {
                fprintf(stderr, "Operation Failed");
                close(write_fd);
                exit(1);
            } else if (bytes_written == 0) {
                break;
            }

            pos += bytes_written;
            requested_bytes_remaining -= bytes_written;
            bytes_to_write -= bytes_written;
        }
    }

    //const char nl = '\n';
    //write(write_fd, &nl, 1);

    close(write_fd);
    printf("OK\n");
    exit(0);
}

void invalid_command(void) {
    fprintf(stderr, "Invalid Command\n");
    exit(1);
}
