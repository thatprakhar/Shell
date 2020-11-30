#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <limits.h>
#include <signal.h>
#include <cstdio>
#include <stdio.h>
#include "command.hh"
#include "shell.hh"


Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _appendOut = false;
    _appendErr = false;
    _pipe = false;
    _ambigousRedirect = false;
    return_code = 0;
    last_bg = 0;
    last_arg = "";
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
    _pipe = false;
    _appendOut = false;
    _appendErr = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}


void Command::execute() {
    // Don't do anything if there are no simple commands

    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    if (_ambigousRedirect) {
        _ambigousRedirect = false;
        printf("Ambiguous output redirect.\n");
        clear();
        Shell::prompt();
        return;
    }


    if (strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "exit") == 0) {
        fprintf(stdout, "Good Bye!\n");
        exit(0);
    }


    // Print contents of Command data structure
    if (isatty(0)) print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec


    // setting default IO
    int default_out = dup(1);
    int default_in = dup(0);
    int default_err = dup(2);

    int fdin = dup(0);
    int err_fd;

    // Setting starting IO
    if (_inFile) {
        fdin = open(_inFile->c_str(), O_RDONLY);
        if (fdin < 0){
            perror("open");
            exit(1);
        }
    }
    if (_errFile) {
        if (_appendErr) {
            err_fd = open(_errFile->c_str(), O_CREAT| O_WRONLY| O_APPEND, 0664);
        } else {
            err_fd = open(_errFile->c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0664);
        }
        if (err_fd < 0){
            perror("open");
            exit(1);
        }
        dup2(err_fd, 2);
        close(err_fd);
    }

    // Executing Commands

    int ret = 0;
    int fdout;
    // bool in_set = false;
    for (int j = 0; j < (int) _simpleCommands.size(); j++) {
        auto command = _simpleCommands[j];
        dup2(fdin, 0);
        close(fdin);
        if (j == (int) _simpleCommands.size()-1) {
            if (_outFile) {
                if (_appendOut) {
                    fdout = open(_outFile->c_str(), O_CREAT| O_WRONLY| O_APPEND, 0664);
                } else {
                    fdout = open(_outFile->c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0664);
                }
                if (fdout < 0){
                    perror("open");
                    exit(1);
                }    
            } else {
                fdout = dup(default_out);
            }
        } else {
            int fdpipe[2];
            pipe(fdpipe);
            fdout=fdpipe[1];
            fdin=fdpipe[0];
        }
        dup2(fdout, 1);
        close(fdout);
        char **args = parse_arguments(command->_arguments);
        if (!strcmp(args[0], "setenv")) {
            setenv(args[1], args[2], 1);
        } else if (!strcmp(args[0], "unsetenv")) {
            unsetenv(args[1]);
        } else if (!strcmp(args[0], "cd")) {
            if (command->_arguments.size() == 1) {
                chdir(getenv("HOME"));
            } else {
                if (args[1][0] == '$') {
                    chdir(getenv(args[1] + 1));
                } else {
                    int status = chdir(args[1]);
                    if (status == -1) {
                        fprintf(stderr, "cd: can't cd to %s\n", args[1]);
                    }
                }
            }
        } else {
            ret = fork();
            if (ret == 0) {
                if (!strcmp(args[0], "printenv")) {
                    char **p = environ;
                    while (*p) {
                        printf("%s\n", *p);
                        p++;
                    }
                    exit(0);
                } else {
                    execvp(args[0], args);
                    perror("execvp");
                    exit(1);
                }
            }
        }
        for (int i = 0; i < (int) command->_arguments.size(); i++) {
            free(args[i]);
            args[i] = NULL;
        }
        free(args);
        args = NULL;
    }

    dup2(default_out, 1);
    dup2(default_in, 0);
    dup2(default_err, 2);
    close(default_in);
    close(default_out);
    close(default_err);
    // Clear to prepare for next command
    if (!_background) {
        int status;
        waitpid(ret, &status, 0);
        if (WIFEXITED(status)) {
            return_code = WEXITSTATUS(status);
        }
    } else {
        last_bg = ret;
        fprintf(stdout, "[%d] exited\n", ret);
    }
    clear();

    // Print new prompt
    if (isatty(0)) Shell::prompt();
}

char** Command::parse_arguments(std::vector<std::string *> _arguments) {
    char **args = (char **) malloc(sizeof(char *) * (_arguments.size() + 1));
    for (int i = 0; i < (int) _arguments.size(); i++) {
        if (*_arguments[i] == "${?}") {
            std::string exit_code = "";
            int s = return_code;
            do {
                exit_code += (s%10 + '0');
                s /= 10;
            } while (s > 0);
            reverse(exit_code.begin(), exit_code.end());
            args[i] = strdup(exit_code.c_str());
        } else if (*_arguments[i] == "${!}") {
            std::string exit_code = "";
            int s = last_bg;
            do {
                exit_code += (s%10 + '0');
                s /= 10;
            } while (s > 0);
            reverse(exit_code.begin(), exit_code.end());
            args[i] = strdup(exit_code.c_str());
        } else if (*_arguments[i] == "${_}") {
            args[i] =  strdup(last_arg.c_str());
        } else  {
            args[i] =  strdup(_arguments[i]->c_str());
        }
        if (i == (int) _arguments.size() - 1) {
            last_arg = std::string(args[i]);
        }
    }
    args[_arguments.size()] = NULL;
    return args;
}

SimpleCommand * Command::_currentSimpleCommand;
