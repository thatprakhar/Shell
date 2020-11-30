#include <cstdio>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "shell.hh"

int yyparse(void);

void Shell::prompt() {
  if (!isatty(0)) return;
  printf("myshell>");
  fflush(stdout);
}

extern "C" void handler(int sig) {
  sig++;
  fprintf(stdout, "\n");
  Shell::prompt();
}

extern "C" void zombie_handler(int sig) {
  sig++;
 	while( waitpid(-1, NULL, WNOHANG) > 0);
}


int main(int argc, char** argv) {

  struct sigaction sa;
  sa.sa_handler = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if(sigaction(SIGINT, &sa, NULL)){
    perror("sigaction");
    exit(2);
  }

  struct sigaction sa_child;
	sa_child.sa_handler = zombie_handler;
	sa_child.sa_flags = SA_RESTART;
	sigemptyset(&sa_child.sa_mask);
	if (sigaction(SIGCHLD, &sa_child, NULL) == -1) {
		perror("sigchild action");
	}
  if (argc > 0) {
    Shell::path = strdup(argv[0]);
  }
  Shell::prompt();
  yyparse();
}

char* Shell::path;

Command Shell::_currentCommand;
