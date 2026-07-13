#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <minix/input.h>

#define COLS 80
#define ROWS 24

static char canvas[ROWS][COLS];
static int cur_x, cur_y;
static int btn_left, btn_right;
static struct termios oldt;

static void cleanup(int sig)
{
	(void)sig;
	printf("\033[?25h\033[2J\033[H");
	fflush(stdout);
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	_exit(0);
}

int main(void)
{
	int fd;
	struct input_event ev;
	struct termios newt;

	fd = open("/dev/mouse0", O_RDONLY);
	if (fd < 0) {
		perror("/dev/mouse0");
		return 1;
	}

	/* Guardar config original del terminal y poner modo raw */
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	/* Restaurar terminal al salir con Ctrl+C */
	signal(SIGINT, cleanup);

	/* Inicializar canvas vacio y limpiar pantalla */
	memset(canvas, ' ', sizeof(canvas));
	printf("\033[2J\033[?25l");

	/* Cursor empieza en el centro */
	cur_x = COLS / 2;
	cur_y = ROWS / 2;
	printf("\033[%d;%dH+", cur_y + 1, cur_x + 1);
	fflush(stdout);

	/* Bucle principal: leer eventos del mouse */
	while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {

		/* Evento de boton */
		if (ev.page == INPUT_PAGE_BUTTON) {
			if (ev.code == INPUT_BUTTON_1)
				btn_left = ev.value;
			else if (ev.code == INPUT_BUTTON_1 + 1)
				btn_right = ev.value;
			continue;
		}

		/* Evento de movimiento */
		if (ev.page == INPUT_PAGE_GD) {
			int nx = cur_x, ny = cur_y;

			if (ev.code == INPUT_GD_X)
				nx += ev.value;
			else if (ev.code == INPUT_GD_Y)
				ny -= ev.value;

			/* Limitar a los bordes de la pantalla */
			if (nx < 0) nx = 0;
			if (nx >= COLS) nx = COLS - 1;
			if (ny < 0) ny = 0;
			if (ny >= ROWS) ny = ROWS - 1;

			if (nx != cur_x || ny != cur_y) {
				/* Restaurar lo que habia bajo el cursor */
				printf("\033[%d;%dH%c",
				    cur_y + 1, cur_x + 1, canvas[cur_y][cur_x]);

				cur_x = nx;
				cur_y = ny;

				/* Brocha o goma */
				if (btn_left)
					canvas[cur_y][cur_x] = '#';
				if (btn_right)
					canvas[cur_y][cur_x] = ' ';

				/* Dibujar cursor en nueva posicion */
				printf("\033[%d;%dH+", cur_y + 1, cur_x + 1);
				fflush(stdout);
			}
		}
	}

	close(fd);
	cleanup(0);
	return 0;
}

