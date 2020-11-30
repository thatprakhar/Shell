
/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include "y.tab.hh"
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "shell.hh"

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

extern char * read_line();

int mygetc(FILE * f) {
 static char *p;
 char ch;

 if (!isatty(0)) {
  // stdin is not a tty. Call real getc
   return getc(f);
 }

 // stdin is a tty. Call our read_line.
 if (p == NULL || *p == 0) {
   char * s = read_line();
   p = s;
 }

 ch = *p;
 p++;

 return ch;
}

#undef getc
#define getc(f) mygetc(f)

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}

">>" {
  return GREATGREAT;
}

"2>" {
  return TWOGREAT;
}

"|" {
  return PIPE;
}

"&" {
  return AMPERSAND;
}

"<" {
  return LESS;
}

">&" {
  return GREATAMPERSAND;
}

">>&" {
  return GREATGREATAMPERSAND;
}

"~" {
  char *home_dir = getenv("HOME");
  for (int i = strlen(home_dir) - 1; i >= 0 ; i--) {
    myunputc(home_dir[i]);
  }
}

"~"[^ \t\n][^ \t\n]* {
  std::string *user = new std::string(yytext+1);
  *user = "/homes/" + *user;
  char *dir = strdup(user->c_str());
  for (int i = strlen(dir) - 1; i >= 0 ; i--) {
    myunputc(dir[i]);
  }
}

[^ \t\n]*\\[^ \t\n]* {
  int i = 0;
  char *word = new char[strlen(yytext)];
  char * args = yytext;

	while (*args) {
		if (*args == '\\') {	
		  word[i] = *(++args);
			i++;	
		} else {
			word[i] = *args;
			i++;
		}
		args++;
	}
	word[i] = '\0';
	yylval.cpp_string = new std::string(word);
	return WORD;
}

["][^\n\"]*["] {
  int i = 0;
  char *words = new char[strlen(yytext)];
  char* args = yytext;

  while (*args) {
    if (*args == '\"') {
      // nothing
    } else {
      words[i] = *args;
      i++;
    }
    args++;
  }
  words[i] = '\0';
  yylval.cpp_string = new std::string(words);
  return WORD;
}

\$\([^\n\$]*\)|`[^\n`]*` {
  // subshell
  std::string *subshell_cmd;
  if (yytext[0] ==  '`') {
		subshell_cmd = new std::string(yytext+1);
		subshell_cmd->pop_back();
	}
	else {
		subshell_cmd = new std::string(yytext+2);
		subshell_cmd->pop_back();
	}
  int default_in = dup(0);
  int default_out = dup(1);

  int pin[2], pout[2];
  pipe(pin);
  pipe(pout);
  write(pin[1], subshell_cmd->c_str(), subshell_cmd->size());
  write(pin[1], "\n", 1);
	close(pin[1]);

  dup2(pin[0], 0);
	close(pin[0]);
	dup2(pout[1], 1);
	close(pout[1]);

  int ret = fork();

  if (ret == 0) {
    execvp("/proc/self/exe", NULL);
    _exit(1);
  } else if (ret < 0) {
		perror("fork");
		exit(1);
	}
  dup2(default_in, 0);
	dup2(default_out, 1);
	close(default_in);
	close(default_out);

  char *buf = new char[3000];
  int i = 0;
  char ch = '\n';

  while (read(pout[0], &ch, 1)) {
		if (ch == '\n') {
      buf[i] = ' ';
    } else {
      buf[i] = ch;
    }
    i++;
	}

  close(pout[0]);

  buf[i] = '\n';
  for (int j = i; j >= 0; j--) {
    myunputc(buf[j]);
  }
}

"${$}" {
  pid_t current_process = getpid();
  char *process_id_string = new char[10];

  int i = 0;
  do {
    process_id_string[i] = (current_process%10 + '0');
    current_process /= 10;
    i++;
  } while (current_process > 0); 

  for (int j = 0; j < i; j++) {
    myunputc(process_id_string[j]);
  }
}

"${SHELL}" {
  char buf[PATH_MAX];
  char *path;
  path = realpath(Shell::path, buf);
  if (path) {
    for (int i = strlen(path) - 1; i >= 0; i--) {
      myunputc(path[i]);
    }
  } else {
    perror("path");
    exit(1);
  }
}

[A-Za-z0-9]*"${"[^ \t\n}!$?_][^ \t\n!?$}_]*"}"[^ \t\n]* {
  char *ptr = yytext;
  std::string str = "";
  int len = strlen(yytext);
  while (*ptr) {
    while (*ptr && *ptr != '$') {
      str += *ptr;
      ptr++;
    }
    if (!*ptr) {
      break;
    }
    ptr += 2;
    std::string env_var = "";
    while (*ptr && *ptr != '}') {
      env_var += *ptr;
      ptr++;
    }
    char *env_name = NULL;
    if (env_var != "SHELL") {
      env_name =  getenv(env_var.c_str());
    } else {
      char buf[PATH_MAX];
      env_name = realpath(Shell::path, buf);
    }
    str += std::string(env_name);
    ptr++;
  }
  while (*ptr) {
    str += *ptr;
    ptr++;
  }

  for (int i = str.length() - 1; i >= 0; i--) {
    myunputc(str[i]);
  }
}

"source "[^ \t\n<>|][^ \t\n<>|]* {
  std::string *file_name = new std::string(yytext + 7);

  std::string source_cmd;
  std::ifstream file_ptr;
  file_ptr.open(*file_name);

  std::vector<string> cmds;
  while (std::getline(file_ptr, source_cmd)) {
    // cout << source_cmd << "\n";
    source_cmd += "\n";
    cmds.push_back(source_cmd);
  }
  file_ptr.close();

  for (int i = cmds.size()-1; i >= 0; i--) {
    for (int j = cmds[i].length()-1; j >= 0; j--) {
      myunputc(cmds[i][j]);
    }
  }
}


[^ \t\n><|][^ \t\n><|]*  {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}
