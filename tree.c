#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

void print_tree(const char *path, int level) {
    struct dirent *entry;
    struct stat statbuf;
    DIR *dir = opendir(path);

    if (!dir) return; // No se pudo abrir el directorio

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

       
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);


        if (lstat(full_path, &statbuf) == -1) continue;

 
        for (int i = 0; i < level; i++) printf("  ");
        
        if (S_ISDIR(statbuf.st_mode)) {
            printf("[%s]\n", entry->d_name);
            print_tree(full_path, level + 1);
        } else {
            printf("%s\n", entry->d_name); 
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    const char *start_path = (argc > 1) ? argv[1] : ".";
    printf("%s\n", start_path);
    print_tree(start_path, 1);
    return 0;
}
