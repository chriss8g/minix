/*
 * Prueba de pthread_mutex_trylock via _MTHREADIFY_PTHREADS.
 * Verifica que la segunda llamada sobre el mismo mutex retorna EDEADLK
 * (o EBUSY), no recursa infinitamente.
 *
 * El valor numerico de EDEADLK depende del sistema: en MINIX/NetBSD es 11,
 * en Linux 35. Lo relevante es el simbolo, no el numero impreso.
 *
 * Salida esperada en MINIX:
 *   first trylock: 0 (OK)
 *   second trylock: 11 (Resource deadlock avoided)
 *   unlock: 0 (OK)
 *   destroy: 0 (OK)
 *   PASS
 */
#define _MTHREADIFY_PTHREADS
#include <minix/mthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static const char *describe(int r)
{
	return (r == 0) ? "OK" : strerror(r);
}

int main(void)
{
	pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
	int r, pass = 1;

	r = pthread_mutex_trylock(&m);
	printf("first trylock: %d (%s)\n", r, describe(r));
	if (r != 0) pass = 0;

	r = pthread_mutex_trylock(&m);
	printf("second trylock: %d (%s)\n", r, describe(r));
	if (r != EDEADLK && r != EBUSY) pass = 0;

	r = pthread_mutex_unlock(&m);
	printf("unlock: %d (%s)\n", r, describe(r));
	if (r != 0) pass = 0;

	r = pthread_mutex_destroy(&m);
	printf("destroy: %d (%s)\n", r, describe(r));
	if (r != 0) pass = 0;

	printf("%s\n", pass ? "PASS" : "FAIL");
	return pass ? 0 : 1;
}
