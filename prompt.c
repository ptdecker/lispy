/* Provide a basic REPL (read, evaluate, print loop) example */

/* Requires sudo apt-get install libedit-dev */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <editline/readline.h>
#include <editline/history.h>

int main (int argc, char** argv) {

	puts("Lispy Version 0.0.1");
	puts("Press 'ctrl-c' to exit\n");

	while (true) {
		char* input = readline("lispy> ");
		add_history(input);
		printf("No, you're a %s\n", input);
		free(input);
	}

	return EXIT_SUCCESS;
}

