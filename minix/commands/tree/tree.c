#include <stdio.h>      // printf, perror
#include <stdlib.h>     // malloc, free, exit
#include <string.h>     // strcmp, strlen
#include <dirent.h>     // opendir, readdir, closedir
#include <sys/stat.h>   // stat, lstat, S_ISDIR, S_ISLNK
#include <unistd.h>     // getcwd, lstat, readlink
#include <limits.h>     

void print_prefix(int levels[], int depth) {
    for (int i = 0; i < depth; i++) {
        if (levels[i])
            printf("│   ");
        else
            printf("    ");
    }
}

void print_node(const char *name, int is_last, int levels[], int depth) {
    print_prefix(levels, depth);

    if (is_last)
        printf("└── %s\n", name);
    else
        printf("├── %s\n", name);
}

char *strip(char* str){
    size_t t = sizeof(str);
    char *newstr = malloc(t);
    if (!newstr) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < t-1; i++){
        if (str[i] == ' ') continue;
        newstr[j] = str[i];
        j++;
    }
    newstr[j] = '\0';
    return newstr;
}
char *actualfile(char* dir){
    char *last = strrchr(dir, '/');
    if (last == NULL) return dir;
    return last+1;
}

void tree_helper(const char *path, int deep , int levels[]){
    struct stat st;
    if (lstat(path, &st) == -1) return;

   const char *name = actualfile((char*)path);

    int is_last = 0; 
    print_node(name, levels[deep] == 0, levels, deep);

    if (!S_ISDIR(st.st_mode)) return;
    DIR* d = opendir(path); 
    if (!d) return;

    struct dirent *child;
    int count = 0;
    int total = 0;

    while ((child = readdir(d)) != NULL) {
        if (strcmp(child->d_name, ".") == 0 || strcmp(child->d_name, "..") == 0)
            continue;
        total++;
    }

    rewinddir(d);

    while((child = readdir(d)) != NULL){                                    //recursividad en cada subcarpeta
        if (strcmp(child->d_name, ".") == 0 || strcmp(child->d_name, "..") == 0) continue;

        count++;
        int is_last_child = (count == total);

        char newpath[PATH_MAX];
        snprintf(newpath, sizeof(newpath), "%s/%s", path, child->d_name); 
        levels[deep] = !is_last_child;
        tree_helper(newpath, deep+1 , levels);
    }
    closedir(d);
}

void tree(char *path){
   int levels[PATH_MAX] = {0};
    if (path == NULL || strip(path)[0] == '\0'){                             //no hay parametro ruta
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL){
            perror("cwd fail");
            return;
        }
        path = cwd;
    }
    else {
        struct stat buffer;
        if (stat(path, &buffer) == -1) {                                         //no existe el directorio
            printf("The path not exists");
            return;
        }
    }
    
    tree_helper(path, 0 , levels);    
}

