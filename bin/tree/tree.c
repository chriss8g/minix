#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

void tree(const char *path, const char *prefijo, int es_ultimo, int profundidad);

void tree(const char *path, const char *prefijo, int es_ultimo, int profundidad) {
    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "tree: %s: %s\n", path, strerror(errno));
        return;
    }

    struct dirent **entries = NULL;
    int n = scandir(path, &entries, NULL, alphasort);
    if (n < 0) {
        fprintf(stderr, "tree: %s: %s\n", path, strerror(errno));
        closedir(d);
        return;
    }

    for (int i = 0; i < n; i++) {
        char *name = entries[i]->d_name;
        //Ignorar "." y ".."
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

        
        struct stat st;
        //Si falla lstat()
        if (lstat(fullpath, &st) == -1) {
            fprintf(stderr, "tree: %s: %s\n", fullpath, strerror(errno));
            continue;
        }

        //Es el ultimo entry?
        int last = (i == n - 1);
        //Cambiar identacion si es el ultimo o no
        const char *branch = last ? "`-- " : "|-- ";
        const char *nextpref = last ? "    " : "|   ";

        //Es un link simbolico?
        if (S_ISLNK(st.st_mode)) {
            printf("%s%s%s (symlink)\n", prefijo, branch, name);
            // No se sigue recursivamente
        
        //Es un directorio?
        } else if (S_ISDIR(st.st_mode)) {
            printf("%s%s%s/\n", prefijo, branch, name);
            char newpref[1024];
            snprintf(newpref, sizeof(newpref), "%s%s", prefijo, nextpref);
            tree(fullpath, newpref, last, profundidad + 1);
        
        //Es un archivo regular?
        } else {
            printf("%s%s%s\n", prefijo, branch, name);
        }
    }

    //LIMPIEZA!
    for (int i = 0; i < n; i++) free(entries[i]);
    free(entries);
    closedir(d);
}

int main(int argc, char **argv) {
    const char *root = (argc > 1) ? argv[1] : ".";
    struct stat st;
    if (lstat(root, &st) == -1) {
        perror(root);
        return 1;
    }
    printf("%s\n", root);
    if (S_ISDIR(st.st_mode)) {
        tree(root, "", 1, 0);
    }
    return 0;
}
