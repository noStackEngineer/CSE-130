#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define BUFFER_SIZE 16

int main(void) {
	char buffer[BUFFER_SIZE];
	const char* delims = "\n";

	while (1) {
		ssize_t bytes_read = read(0, buffer, BUFFER_SIZE);

		if (bytes_read < 0) {
			printf("error on read!\n");
			exit(1);
		}
		if (bytes_read == 0) {
			printf("no bytes were read\n");
			break;
		}

		printf("%zi bytes were successfully read!\n", bytes_read);
	}

	return 0;
}
