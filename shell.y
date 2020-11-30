
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{
#include <string>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <sys/types.h>
#include <regex.h>
#include <dirent.h>
#include <signal.h>
using namespace std;
#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE AMPERSAND GREATAMPERSAND GREATGREAT LESS PIPE GREATGREATAMPERSAND TWOGREAT

%{
//#define yylex yylex
#include <cstdio>
#include "shell.hh"

void yyerror(const char * s);
int yylex();
std::vector<string> matches;

static bool compare(string &a, string &b) {
  return a < b;
}

void expandWildcardsIfNecessary(std::string *argument, std::string path, int index) {
  // recursively call for each directory that matches
  // std::cout << *argument << " " << path << " " << index <<" \n";
  const char *arg = argument->substr(index, argument->length() - index).c_str();
  // printf("%s\n", arg);
  // std::cout << "got " << *argument << "\n";
  if (!strchr(arg, '*') && !strchr(arg, '?')) {
    // std::cout << "no wildcard\n";
    // Command::_currentSimpleCommand->insertArgument(argument);
    return;
  }

  // std::cout << "path is " << path << "\n";
  bool hasWildCard = false;
  std::string temp_path = "";
  int done_till = index;
  if (arg[0] == '/') {
    expandWildcardsIfNecessary(argument, path + "/", index + 1);
    return;
  } else {
    char *temp_char_array = strdup(arg);
    char *found = strchr(temp_char_array, '/');
    free(temp_char_array);
    temp_char_array = NULL;
    if (found) {
      const char *temp = arg;
      while (*temp != '/') {
        temp_path += *temp;
        if (*temp == '*' || *temp == '?') {
          hasWildCard = true;
        }
        temp++;
      }
      if (!hasWildCard) {
        expandWildcardsIfNecessary(argument, path + temp_path, index + (int)(temp - arg));
        return;
      }
    }
  }

  char *reg = (char *) malloc(2 * strlen(arg) + 10);
  const char *a = temp_path != "" ? temp_path.c_str() : arg;
  char *r = reg;
  *r = '^';
  r++;

  while (*a) {
    if (*a == '*') {
      *r = '.';
      r++;
      *r = '*';
      r++;
    } else if (*a == '?') {
      *r = '.';
      r++;
    } else if (*a == '.') {
      *r = '\\';
      r++;
      *r = '.';
      r++;
    } else {
      *r = *a;
      r++;
    }
    a++;
    index++;
  }
  *r = '$';
  r++;
  *r = 0;

  regex_t re;
  int expbuf = regcomp( &re, reg,  REG_EXTENDED|REG_NOSUB);
	if (expbuf != 0) {
    printf("error here\n");
		perror("compile");
    return;
  }

  DIR *curr_dir = NULL;

  if (path != "") {
    curr_dir = opendir(path.c_str());
  } else {
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    curr_dir = opendir(cwd);
  }
  if (curr_dir == NULL) {
    // perror("opendir");
    return;
  }
  struct dirent *ent;
  while ((ent = readdir(curr_dir)) != NULL) {
    if (regexec(&re, ent->d_name, (size_t)0, NULL, 0) == 0) {
      // Command::_currentSimpleCommand->insertArgument(new std::string(ent->d_name));
      if (ent->d_name[0] != '.') {
        if (index == argument->length()) {
          // printf("adding: %s\n", ent->d_name);
          matches.push_back(path + std::string(ent->d_name));
        } else {
          // printf("expanding in %s with done = %d\n", ent->d_name, index);
          expandWildcardsIfNecessary(argument, path + std::string(ent->d_name), index);
        }
      }
    }
  }
  free(reg);
  regfree(&re);
  closedir(curr_dir);
}

%}

%%

goal:
  command_list
  ;

command_list: 
  command_list command_line
  | /* command loop */
  ;

command_line:	
  pipe_list iomodifier_list background_optional NEWLINE {
    // cout << "Yacc: Executing: \n";
    Shell::_currentCommand.execute();
  }
  | NEWLINE {
    Shell::prompt();
  }
  | error NEWLINE { yyerrok; }
  ;

command_and_args:
  command_word argument_list {
    Shell::_currentCommand.insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

argument_list:
  argument_list argument
  | /* can be empty */
  ;

argument:
  WORD {
    // printf("  Yacc: insert argument \"%s\"\n", $1->c_str());
    // Command::_currentSimpleCommand->insertArgument( $1 );
    if (*($1) != "${?}") {
      expandWildcardsIfNecessary($1, "", 0);
      if (matches.size() == 0) {
        Command::_currentSimpleCommand->insertArgument( $1 );
      } else {
        sort(matches.begin(), matches.end(), compare);
        for (auto &a : matches) {
          Command::_currentSimpleCommand->insertArgument(new std::string(a));
        }
        matches.clear();
      }
    } else {
      Command::_currentSimpleCommand->insertArgument( $1 );
    }
  }
  ;

command_word:
  WORD {
    // printf("  Yacc: insert command \"%s\"\n", $1->c_str());
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument($1);
  }
  ;

pipe_list:
  pipe_list PIPE command_and_args {
    Shell::_currentCommand._pipe = true;
  }
  | command_and_args
  ;

iomodifier_list:
  iomodifier_list iomodifier_opt
  | /* Empty string */
  ;

iomodifier_opt:
  GREAT WORD {
    // printf("  Yacc: insert output \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._outFile) {
      Shell::_currentCommand._ambigousRedirect = true;
    }
    Shell::_currentCommand._outFile = $2;
  }
  | GREATGREAT WORD {
    // cout << "  Yacc: insert output >> \"" << $2->c_str() <<"\"\n";
    if (Shell::_currentCommand._outFile) {
      Shell::_currentCommand._ambigousRedirect = true;
    }
    Shell::_currentCommand._appendOut = true;
    Shell::_currentCommand._outFile = $2;
  }
  | GREATGREATAMPERSAND WORD {
    // cout << "  Yacc: insert >>& \"" << $2->c_str() << "\"\n";
    if (Shell::_currentCommand._outFile || Shell::_currentCommand._errFile ) {
      Shell::_currentCommand._ambigousRedirect = true;
    }
    Shell::_currentCommand._appendOut = true;
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._appendErr = true;
    Shell::_currentCommand._errFile = $2;
  }
  | GREATAMPERSAND WORD {
    // cout << " Yacc: insert >& \"" << $2->c_str() << "\"\n";
    if (Shell::_currentCommand._outFile || Shell::_currentCommand._errFile ) {
      Shell::_currentCommand._ambigousRedirect = true;
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
  }
  | LESS WORD {
    // cout << "  Yacc: insert input < \"" << $2->c_str() << "\"\n";
    if (Shell::_currentCommand._inFile) {
      Shell::_currentCommand._ambigousRedirect = true;
    }
    Shell::_currentCommand._inFile = $2;
  }
  | TWOGREAT WORD {
    // cout << " Yacc: insert error to " << $2->c_str() << "\n";
    if (Shell::_currentCommand._errFile) {
      Shell::_currentCommand._ambigousRedirect = true;
    }
    Shell::_currentCommand._errFile = $2;
  } 
  ;

background_optional:
  AMPERSAND {
    // cout << "  Yacc: insert background\n";
    Shell::_currentCommand._background = true;
  }
  | /*  empty string */
  ;

%%

void
yyerror(const char * s)
{
  fprintf(stderr,"here: %s", s);
  Shell::prompt();
}

#if 0
main()
{
  yyparse();
}
#endif
