/*** includes ***/
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/
struct editorConfig {
	int screenrows;
	int screencols;
	struct termios orig_termios;
};
struct editorConfig E;
	
/*** terminal ***/
void die(const char *s){
	write(STDOUT_FILENO , "\x1b[2J" , 4);
	write(STDOUT_FILENO , "\x1b[H" , 3);
	perror(s);
	exit(1);
}
//Disabling the raw mode on exit:
void disableRawMode(){
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		die("tcsetattr");
}
// Enabling the raw mode bcs there is no built in such thing
void enableRawMode(){
	if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);
	struct termios raw = E.orig_termios;
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
int getWindowSize(int *rows , int *cols){
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		return -1;
	} else {
	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return 0;
	}
}

/*** output ***/
void editorDrawRows(){
	int y;
	for(y = 0 ; y < E.screenrows; y++){
		write(STDIN_FILENO, "~\r\n" , 3);
	}
}
void editorRefreshScreen(){
	write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
	editorDrawRows();
        write(STDOUT_FILENO, "\x1b[H", 3);
}


/*** input ***/
void editorProcessKeypress(){
	char c = editorRoadKey();

	switch(c){
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	}
}

/*** init ***/
void initEditor(){
	if(getWindowSize(&E.screenrows , &E.screencols) == -1) die("WindowsSize");
}
int main(){
	enableRawMode();
	initEditor();
	while (1){	
		editorRefreshScreen();
		editorProcessKeypress();
	};
	return 0;
}
