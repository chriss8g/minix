/*	tree - imprime recursivamente la estructura de directorios.
 *
 * Uso:	tree [ruta]
 *	Sin argumento usa el directorio actual (".").
 *
 * Restricciones del proyecto (Hito 2b, THE MATCOM MINIX):
 *	- Solo llamadas al sistema: opendir/readdir/closedir y lstat.
 *	- No seguir enlaces simbolicos: se detectan con lstat + S_ISLNK
 *	  y no se recursa dentro de ellos (evita ciclos).
 *	- No se invocan herramientas externas (find, ls, etc.).
 *
 * La profundidad se muestra por indentacion con conectores ASCII al
 * estilo del comando tree clasico:
 *
 *	punto
 *	|-- subdir
 *	|   `-- archivo
 *	`-- otro
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Tamano del buffer de prefijo de indentacion. Cada nivel de profundidad
 * agrega 4 caracteres; este limite admite arboles muy profundos y snprintf
 * trunca de forma segura si se excediera. */
#define PREFIX_MAX 4096

static unsigned long n_dirs;	/* directorios visitados (sin contar la raiz) */
static unsigned long n_files;	/* entradas que no son directorio */

/* Comparador para qsort: orden alfabetico estable de nombres. */
static int
cmp_name(const void *a, const void *b)
{
	const char *const *pa = (const char *const *) a;
	const char *const *pb = (const char *const *) b;

	return strcmp(*pa, *pb);
}

/* Recorre y lista el contenido del directorio "path". "prefix" es la
 * cadena de barras/espacios acumulada que dibuja las ramas a la izquierda. */
static void
walk(const char *path, const char *prefix)
{
	DIR *dp;
	struct dirent *de;
	char **names;
	size_t n, cap, i;

	dp = opendir(path);
	if (dp == NULL) {
		/* Permiso denegado, no es directorio, etc.: se reporta y
		 * se continua con el resto del arbol. */
		fprintf(stderr, "tree: %s: %s\n", path, strerror(errno));
		return;
	}

	/* Fase 1: recolectar los nombres (saltando "." y "..") en un
	 * arreglo dinamico, para poder ordenarlos y saber cual es el ultimo. */
	names = NULL;
	n = 0;
	cap = 0;
	while ((de = readdir(dp)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;

		if (n == cap) {
			char **grown;

			cap = (cap == 0) ? 16 : cap * 2;
			grown = realloc(names, cap * sizeof(*names));
			if (grown == NULL) {
				fprintf(stderr, "tree: sin memoria\n");
				break;
			}
			names = grown;
		}
		names[n] = strdup(de->d_name);
		if (names[n] == NULL) {
			fprintf(stderr, "tree: sin memoria\n");
			break;
		}
		n++;
	}
	closedir(dp);

	qsort(names, n, sizeof(*names), cmp_name);

	/* Fase 2: imprimir cada entrada y recursar en los subdirectorios. */
	for (i = 0; i < n; i++) {
		char full[PATH_MAX];
		struct stat st;
		int last;
		const char *connector;

		last = (i + 1 == n);
		connector = last ? "`-- " : "|-- ";

		(void) snprintf(full, sizeof(full), "%s/%s", path, names[i]);

		if (lstat(full, &st) == -1) {
			printf("%s%s%s  [%s]\n", prefix, connector,
			    names[i], strerror(errno));
			free(names[i]);
			continue;
		}

		if (S_ISLNK(st.st_mode)) {
			/* Enlace simbolico: mostrar destino, NO recursar. */
			char target[PATH_MAX];
			ssize_t len;

			len = readlink(full, target, sizeof(target) - 1);
			if (len >= 0) {
				target[len] = '\0';
				printf("%s%s%s -> %s\n", prefix, connector,
				    names[i], target);
			} else {
				printf("%s%s%s\n", prefix, connector, names[i]);
			}
			n_files++;
		} else if (S_ISDIR(st.st_mode)) {
			char nprefix[PREFIX_MAX];

			printf("%s%s%s/\n", prefix, connector, names[i]);
			n_dirs++;

			/* El nuevo prefijo continua la barra vertical si esta
			 * entrada no es la ultima; si lo es, deja espacio. */
			(void) snprintf(nprefix, sizeof(nprefix), "%s%s",
			    prefix, last ? "    " : "|   ");
			walk(full, nprefix);
		} else {
			printf("%s%s%s\n", prefix, connector, names[i]);
			n_files++;
		}

		free(names[i]);
	}

	free(names);
}

int
main(int argc, char *argv[])
{
	const char *root;
	struct stat st;

	root = (argc > 1) ? argv[1] : ".";

	if (lstat(root, &st) == -1) {
		fprintf(stderr, "tree: %s: %s\n", root, strerror(errno));
		return 1;
	}

	/* La raiz se imprime tal cual fue dada en la linea de comandos. */
	printf("%s\n", root);

	if (S_ISDIR(st.st_mode))
		walk(root, "");

	printf("\n%lu directories, %lu files\n", n_dirs, n_files);
	return 0;
}
