#ifndef shell_hh
#define shell_hh

#include "command.hh"

struct Shell {
  static void prompt();
  static char* path;
  static Command _currentCommand;
};

#endif
