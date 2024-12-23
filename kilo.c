/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"
#define ABUF_INIT {NULL , 0 }

enum editorKey{
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};
/*** data ***/ 
typedef struct erow{
	int size;
	char *chars;
}erow; //Editor rows
struct editorConfig {
	int cx; //cursor column position
	int cy; //cursor row position
	int screenrows;
	int screencols;
	int numrows;
	erow *row; // Making E.row an array of erow
	int rowoff;
	struct termios orig_termios; //This is where we initiate the original terminal
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
int editorRoadKey(){
	int nread;
	char c; 
	while((nread = read(STDIN_FILENO, &c , 1)) != 1){
		if(nread == -1 && errno != EAGAIN) die ("read");
	} 
	if(c == '\x1b'){
		char seq[3];

		if(read(STDIN_FILENO, &seq[0] , 1) != 1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1] , 1) != 1) return '\x1b';
		
		if(seq[0] == '['){
			if(seq[1] > 0 && seq[1] <= '9'){
				if (read(STDIN_FILENO , &seq[2] , 1) != 1) return '\x1b';
				if (seq[2] == '~'){
					switch(seq[1]){
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_UP;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			}else{
			switch(seq[1]){
			case'A': return ARROW_UP;
			case'B': return ARROW_DOWN;
			case'C': return ARROW_RIGHT;
			case'D': return ARROW_LEFT;
			case'H': return HOME_KEY;
			case'F': return END_KEY;
			}
			}
		}else if(seq[0] == '0'){
				switch(seq[1]){
				case'H': return HOME_KEY;
				case'F': return END_KEY;
				}
			}
		
		return '\x1b';
	}else{
		return c;
	}
}
int getWindowSize(int *rows , int *cols){
	//Here we get infos about are window size and all
	//to try and make our UI better and also handle navigation
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		return -1;
	} else {
	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return 0;
	}
}

/*** row operations ***/
void editorAppendRow(char *s , size_t len){
	E.row = realloc(E.row , sizeof(erow) * (E.numrows + 1));
	
	int at = E.numrows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars , s, len);
	E.row[at].chars[len] = '\0';
	E.numrows++;
}


/*** file i/o ***/
void editorOpen(char *filename){
	FILE *fp = fopen(filename,"r");
	if(!fp) die("fopon");
	char *line = NULL; 
	size_t linecap = 0;
	ssize_t linelen;
	while((linelen = getline(&line, &linecap, fp)) != -1){
	while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
			linelen--;
	editorAppendRow(line , linelen);
	}
	free(line);
	fclose(fp);
}



/*** append buffer ***/
/*** MUST KNOW LINE BY LINE ***/
struct abuf{
	char *b;
	int len;
};
void abAppend(struct abuf *ab, const char *s, int len){
	char *new = realloc(ab->b , ab->len + len);
	if(new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}
void abFree(struct abuf *ab){
	free(ab->b);
}

/*** output ***/
void editorScroll(){
	if(E.cy < E.rowoff){
		E.rowoff = E.cy;
	}
	if(E.cy  >= E.rowoff + E.screenrows){
		E.rowoff = E.cy + E.screenrows + 1;
	} 
}
void editorDrawRows(struct abuf *ab){
	int y;
	for(y = 0 ; y < E.screenrows; y++){
		int filerow = y + E.rowoff;
		if(y>=E.numrows){
		if(E.numrows == 0 && y == E.screenrows / 3){ // if we load a file => then we have rows so we dont display the welcome message
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome), "Hloba editor -- version %s", KILO_VERSION);
			if(welcomelen > E.screencols) welcomelen = E.screencols;
			int padding = (E.screencols - welcomelen) / 2;
			if(padding){
				abAppend(ab , "~" ,1);
				padding--;
			}
			while(padding--) abAppend(ab , " " , 1);
			abAppend(ab , welcome , welcomelen);
		}
		else {
			abAppend(ab, "~" , 1);
		}
		}
		else {
			int len = E.row[filerow].size;
			if( len > E.screencols ) len = E.screencols;
			abAppend(ab , E.row[filerow].chars , len);
		}
		abAppend(ab, "\x1b[K", 3);
		if(y < E.screenrows - 1){
		abAppend(ab, "\r\n"  , 2);
		}
	}
}
void editorRefreshScreen(){
	editorScroll();
	struct abuf ab = ABUF_INIT;
	
	abAppend(&ab, "\x1b[?25l", 6);
        abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf , sizeof(buf) , "\x1b[%d;%dH" , E.cy + 1 , E.cx + 1);
	abAppend(&ab , buf , strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);
	write(STDOUT_FILENO , ab.b, ab.len);
	abFree(&ab);
}


/*** input ***/
void editorMoveCursor(int key){
	switch(key){
		case ARROW_LEFT:
		if(E.cx != 0) {E.cx--;}
		break;
		case ARROW_RIGHT:
		if(E.cx != E.screencols - 1) {E.cx++;}
		break;
		case ARROW_UP:
		if(E.cy != 0) {E.cy--;}
		break;
		case ARROW_DOWN:
		if(E.cy < E.numrows) {E.cy++;}
		break;
	}
}
void editorProcessKeypress(){
	int c = editorRoadKey();
	switch(c){
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
		case HOME_KEY:
		E.cx = 0;
		break;
		case END_KEY:
		E.cx = E.screencols - 1;
		break;
		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = E.screenrows;
				while (times--){
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
				}
			}
		break;
		case ARROW_UP:
		case ARROW_LEFT:
		case ARROW_RIGHT:
		case ARROW_DOWN:
			editorMoveCursor(c);
			break;
	}
}
/*** init ***/
void initEditor(){
	E.cx = 0;
	E.cy = 0;
	E.numrows = 0;
	E.row = NULL;
	E.rowoff = 0;
	if(getWindowSize(&E.screenrows , &E.screencols) == -1) die("WindowsSize");
}
int main(int argc , char *argv[]){
	enableRawMode();
	initEditor();
	if(argc >= 2){
		editorOpen(argv[1]);
	}
	while (1){	
		editorRefreshScreen();
		editorProcessKeypress();
	};
	return 0;
}
