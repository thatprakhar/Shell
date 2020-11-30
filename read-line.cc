/*
 * CS252: Systems Programming
 * Purdue University
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <unistd.h>

#define MAX_BUFFER_LINE 2048

extern "C" void tty_raw_mode(void);

// Buffer where line is stored
int line_length;
int cursor_pos;
char line_buffer[MAX_BUFFER_LINE];

// Simple history array
// This history does not change. 
// Yours have to be updated.
int history_index = 0;

std::vector<char*> history;

void read_line_print_usage() {
  std::string usage = "\nctrl-?       Print usage\nBackspace    Deletes last character\nup arrow     See last command in the history\n";
  write(1, usage.c_str(), usage.length());
}

/* 
 * Input a line with some basic editing.
 */
char * read_line() {
  history.push_back("");
  // Set terminal in raw mode
  tty_raw_mode();

  line_length = 0;
  cursor_pos = 0;

  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);

    if (ch>=32 && ch != 127) {
      // It is a printable character. 
      if (cursor_pos == line_length) {
        // at the end of line
        // Do echo
        write(1,&ch,1);

        // If max number of character reached return.
        if (line_length==MAX_BUFFER_LINE-2) break; 
				// add char to buffer.
				line_buffer[cursor_pos]=ch;
				line_length++;
				cursor_pos++;
      } else {
        // middle of the line
        char *temp = new char[MAX_BUFFER_LINE];
        for (int i = 0; i < MAX_BUFFER_LINE; i++) {
          if (line_buffer[cursor_pos + i] == '\0' ) {
						break;
					}
					temp[i] = line_buffer[cursor_pos + i];
				}
        write(1, &ch, 1);
        line_buffer[cursor_pos] = ch;
				line_length++;
				cursor_pos++;
        int len = 0;
        for (int i = 0; i < MAX_BUFFER_LINE; i++) {
				  len++;
					write(1, &temp[i], 1);
					if (line_buffer[cursor_pos + i] == '\0' ) {
						break;
					}
				}
        for (int i = 0; i < len; i++) {
          ch = 8;
          write(1, &ch, 1);
        }
      }
    } else if (ch==10) {
      // <Enter> was typed. Return line
      if (strlen(line_buffer) > 0) {
        history.push_back(line_buffer);
        history_index++;
      }
      // Print newline
      write(1,&ch,1);
      break;
    } else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0] = 0;
      break;
    } else if (ch == 8 || ch == 127) {
      // <backspace> was typed. Remove previous character read.
      if (cursor_pos < 0) {
        continue;
      }
      // Go back one character
      ch = 8;
      write(1,&ch,1);

      // Write a space to erase the last character read
      ch = ' ';
      write(1,&ch,1);

      // Go back one character
      ch = 8;
      write(1,&ch,1);

      // Remove one character from buffer
      line_buffer[cursor_pos] = '\0';
      line_length--;
      cursor_pos--;
    }
    else if (ch==27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1; 
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1 == 91 && ch2 == 65) {
	      // Up arrow. Print next line in history.
	      // Erase old line
	      // Print backspaces
        int i = 0;
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        // Print spaces on top
        for (i = 0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }

        // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }	

        // Copy line from history
        strcpy(line_buffer, history[history_index]);
        line_length = strlen(line_buffer);
        history_index=(history_index+1)%history.size();

        // echo line
        write(1, line_buffer, line_length);
      } else if (ch1 == 91 && ch2 == 68) {
        // left key
        if (cursor_pos <= 0) continue;
        ch = 8;
        write(1, &ch, 1);
      } else if (ch1 == 91 && ch2 == 67) {
        // rifht key
        if (cursor_pos < line_length) {
          char c = line_buffer[cursor_pos];
				  write(1, &c, 1);
					cursor_pos++;
        }
      } else if (ch1 == 91 && ch2 == 66) {
        // down key
        for (int i = 0; i < line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }
				for (int i =0; i < line_length; i++) {
					ch = ' ';
					write(1,&ch,1);
				}
        for (int i = 0; i < line_length; i++) {
          ch = 8;
          write(1, &ch, 1);
        }
        if (history_index <= 0) {
					history_index = 0;
				} else {
					history_index += 1;
					if (history_index >= (int) history.size() ) {
						history_index = 0;
					} 
				}
        if (history[history_index] == NULL) {
					const char * empty = "";
					strcpy(line_buffer, empty);
				} else {
					strcpy(line_buffer, history[history_index]);
				}
				line_length = strlen(line_buffer);
				write(1, line_buffer, line_length);
      }
    }

  }

  // Add eol and null char at the end of string
  line_buffer[line_length]=10;
  line_length++;
  line_buffer[line_length]=0;

  return line_buffer;
}

