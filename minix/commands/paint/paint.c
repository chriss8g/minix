/*	paint - pizarra de caracteres controlada por el mouse.
 *
 * Uso:	paint
 *
 * Trata la pantalla como una matriz bidimensional de celdas (una por
 * caracter).  Muestra un CURSOR en el centro que se mueve en la misma
 * direccion que el mouse.  Con el boton izquierdo presionado escribe
 * caracteres por donde pasa el cursor (brocha); con el boton derecho
 * escribe espacios (goma).  Se sale con 'q', ESC o Ctrl-C.
 *
 * Restricciones del proyecto (Extra, THE MATCOM MINIX):
 *	- Lee la informacion capturada por el driver del mouse leyendo
 *	  eventos crudos (struct input_event) del nodo /dev/mouse0, que
 *	  expone el input server a partir del driver pckbd (PS/2).
 *	- Dibuja con secuencias de escape ANSI/VT100 por stdout; no accede
 *	  a la memoria de video ni a puertos de hardware.
 *
 * El movimiento del mouse llega como eventos relativos (INPUT_FLAG_REL)
 * de las paginas General Desktop (X/Y) y Button (botones 1=izq, 2=der).
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <minix/input.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#define MOUSE_DEV	"/dev/mouse0"	/* primer mouse multiplexado por input */

#define BRUSH		'*'		/* caracter de la brocha */
#define ERASER		' '		/* la goma escribe espacios */
#define CURSOR		'+'		/* representacion del cursor */

#define DEF_ROWS	25		/* tamano por defecto si falla ioctl */
#define DEF_COLS	80

#define IDLE_USEC	15000		/* pausa cuando no hay eventos (~66 Hz) */

/* Convencion del mouse: un valor Y positivo significa "hacia arriba".
 * En la pantalla la fila 0 esta arriba, asi que subir el mouse debe
 * disminuir la fila.  Si el diagnostico mostrara lo contrario, basta
 * cambiar este 1 por 0. */
#define MOUSE_Y_UP_IS_POSITIVE	1

static struct termios orig_termios;	/* estado original del terminal */
static int termios_saved = 0;		/* 1 si orig_termios es valido */

static int rows = DEF_ROWS;		/* filas de la pantalla */
static int cols = DEF_COLS;		/* columnas de la pantalla */
static char *board = NULL;		/* lienzo: rows*cols celdas */

static int cy, cx;			/* posicion actual del cursor */
static int boton_izq = 0;		/* brocha activa */
static int boton_der = 0;		/* goma activa */

/* Restaura el terminal a su estado original y limpia la pantalla. */
static void
restore_terminal(void)
{
	if (termios_saved)
		(void) tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

	/* limpia la pantalla y manda el cursor a la esquina superior */
	(void) write(STDOUT_FILENO, "\033[2J\033[H", 7);
}

/* Manejador de senales: restaura el terminal y termina.  Necesario para
 * que un kill (o Ctrl-C si quedara activo) no deje la consola en raw. */
static void
on_signal(int sig)
{
	restore_terminal();
	_exit(128 + sig);
}

/* Pone el terminal en modo "raw" a mano (sin cfmakeraw, para no depender
 * de funciones que podrian faltar en la libc instalada). */
static int
set_raw(void)
{
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &orig_termios) < 0)
		return -1;
	termios_saved = 1;

	raw = orig_termios;
	/* sin canonico, sin eco, sin senales de teclado, sin extensiones */
	raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
	/* sin traducir CR->NL ni control de flujo por Ctrl-S/Ctrl-Q */
	raw.c_iflag &= ~(ICRNL | IXON);
	/* read() devuelve enseguida con lo que haya (no bloquea) */
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0)
		return -1;

	return 0;
}

/* Posiciona el cursor del terminal en (y,x) 1-based y escribe un caracter.
 * Evita la esquina inferior derecha: escribir ahi puede hacer scroll de la
 * pantalla en consolas con auto-wrap. */
static void
draw_cell(int y, int x, char ch)
{
	if (y == rows - 1 && x == cols - 1)
		return;
	printf("\033[%d;%dH%c", y + 1, x + 1, ch);
}

/* Si hay un boton presionado, escribe su caracter en la celda actual,
 * tanto en el lienzo (estado) como en la pantalla. */
static void
apply_paint(void)
{
	char ch;

	if (boton_izq)
		ch = BRUSH;
	else if (boton_der)
		ch = ERASER;
	else
		return;

	board[cy * cols + cx] = ch;
	draw_cell(cy, cx, ch);
}

/* Mueve el cursor 'delta' celdas en un eje (axis 0 = x, 1 = y), de a una
 * celda, pintando cada celda del trayecto si hay un boton presionado.  Asi
 * un trazo rapido no deja huecos. */
static void
step_axis(int axis, int delta)
{
	int dir, steps, i;

	dir = (delta > 0) ? 1 : -1;
	steps = (delta > 0) ? delta : -delta;

	for (i = 0; i < steps; i++) {
		/* restaura la celda que el cursor abandona */
		draw_cell(cy, cx, board[cy * cols + cx]);

		if (axis == 0) {
			cx += dir;
			if (cx < 0)
				cx = 0;
			else if (cx > cols - 1)
				cx = cols - 1;
		} else {
			cy += dir;
			if (cy < 0)
				cy = 0;
			else if (cy > rows - 1)
				cy = rows - 1;
		}

		apply_paint();
	}
}

/* Dibuja el cursor sobre la celda actual y deja ahi el cursor de hardware.
 * El lienzo conserva el contenido real, que se restaura al moverse. */
static void
draw_cursor(void)
{
	draw_cell(cy, cx, CURSOR);
	printf("\033[%d;%dH", cy + 1, cx + 1);
	(void) fflush(stdout);
}

/* Procesa un evento del mouse.  Devuelve 1 si cambio algo que haya que
 * redibujar. */
static int
handle_event(const struct input_event *ev)
{
	if (ev->page == INPUT_PAGE_GD) {
		if (ev->code == INPUT_GD_X) {
			step_axis(0, ev->value);
			return 1;
		}
		if (ev->code == INPUT_GD_Y) {
#if MOUSE_Y_UP_IS_POSITIVE
			step_axis(1, -ev->value);
#else
			step_axis(1, ev->value);
#endif
			return 1;
		}
		return 0;
	}

	if (ev->page == INPUT_PAGE_BUTTON) {
		int pressed = (ev->value == INPUT_PRESS);

		if (ev->code == INPUT_BUTTON_1) {		/* izquierdo */
			boton_izq = pressed;
			apply_paint();
			return 1;
		}
		if (ev->code == INPUT_BUTTON_1 + 1) {		/* derecho */
			boton_der = pressed;
			apply_paint();
			return 1;
		}
		return 0;
	}

	return 0;
}

/* Determina el tamano de la pantalla; si no se puede, asume 80x25. */
static void
get_screen_size(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
	    ws.ws_row > 0 && ws.ws_col > 0) {
		rows = ws.ws_row;
		cols = ws.ws_col;
	}
}

int
main(void)
{
	struct input_event ev;
	ssize_t n;
	int mfd;
	char c;

	get_screen_size();

	board = malloc((size_t) rows * cols);
	if (board == NULL) {
		fprintf(stderr, "paint: sin memoria para %dx%d\n", rows, cols);
		return 1;
	}
	memset(board, ' ', (size_t) rows * cols);

	mfd = open(MOUSE_DEV, O_RDONLY | O_NONBLOCK);
	if (mfd < 0) {
		fprintf(stderr, "paint: no se pudo abrir %s: %s\n",
		    MOUSE_DEV, strerror(errno));
		free(board);
		return 1;
	}

	/* manejadores para restaurar el terminal ante senales externas */
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGHUP, on_signal);

	if (set_raw() < 0) {
		fprintf(stderr, "paint: no se pudo poner el terminal en raw\n");
		close(mfd);
		free(board);
		return 1;
	}

	/* pantalla limpia y cursor en el centro */
	cy = rows / 2;
	cx = cols / 2;
	printf("\033[2J");
	draw_cursor();

	for (;;) {
		int changed = 0;

		/* teclado: salir con q/Q, ESC (0x1b) o Ctrl-C (0x03) */
		while (read(STDIN_FILENO, &c, 1) == 1) {
			if (c == 'q' || c == 'Q' || c == 27 || c == 3)
				goto done;
		}

		/* drena todos los eventos del mouse disponibles */
		while ((n = read(mfd, &ev, sizeof(ev))) == (ssize_t) sizeof(ev)) {
			if (handle_event(&ev))
				changed = 1;
		}

		if (changed)
			draw_cursor();
		else
			usleep(IDLE_USEC);
	}

done:
	restore_terminal();
	close(mfd);
	free(board);
	return 0;
}
