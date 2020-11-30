#ifndef command_hh
#define command_hh

#include "simpleCommand.hh"

// Command Data Structure

struct Command {
  std::vector<SimpleCommand *> _simpleCommands;
  std::string * _outFile;
  std::string * _inFile;
  std::string * _errFile;
  bool _background;
  bool _appendOut;
  bool _appendErr;
  bool _pipe;
  bool _ambigousRedirect;
  int return_code;
  int last_bg;
  std::string last_arg;

  Command();
  void insertSimpleCommand( SimpleCommand * simpleCommand );

  void clear();
  void print();
  void execute();
  static SimpleCommand *_currentSimpleCommand;
  char** parse_arguments(std::vector<std::string *> args);
};

#endif
