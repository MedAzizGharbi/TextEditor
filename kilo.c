/*** includes ***/
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/
struct termios orig_termios;

/*** terminal ***/
void die(const char *s){
	perror(s);
	exit(1);
}
//Disabling the raw mode on exit:
void disableRawMode(){
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetattr");
}
// Enabling the raw mode bcs there is no built in such thing
void enableRawMode(){
	if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);
	struct termios raw = orig_termios;
	//THis ICANON gets rid of the canon way so we can read
	//byte by byte and not line by line so we can exit right
	//when q is pressed
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO , TCSAFLUSH , &raw);
}
char editorRoadKey(){
	int nread;
	char c; 
	while((nread = read(STDIN_FILENO, &c , 1)) != 1){
		if(nread == -1 && errno != EAGAIN) die ("read");
	} 
	return c;
}

/*** output ***/
void editorRefreshScreen(){
	write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/
void editorProcessKeypress(){
	char c = editorRoadKey();

	switch(c){
		case CTRL_KEY('q'):
			exit(0);
			break;
	}
}

/*** init ***/
int main(){
	enableRawMode();
	while (1){
		editorRefreshScreen();
		editorProcessKeypress();
	};
	return 0;
}
