#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


static void tree( const char *path, int depth)
{
    DIR *dir=opendir(path);
    struct dirent *entry;
    struct stat st;
    char path1[1024];


    if(dir==NULL)
    {
        printf("    ");
        return;

    }

    entry=readdir(dir);

    while((entry=readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
        {
            continue;
        }

        snprintf(path1, sizeof(path1), "%s/%s", path, entry->d_name);

        if (lstat(path1, &st) == -1) {
            fprintf(stderr, "tree: error '%s': %s\n", path1, strerror(errno));
            continue;
        }


        for (int i = 0; i < depth; i++)
            printf("    ");
        printf("|-- %s\n", entry->d_name);

               if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
            tree(path1, depth + 1);
        }
    }


    closedir(dir);




}







int main(int argc, char **argv) 
{
    const char *root = (argc > 1) ? argv[1] : ".";

    printf("%s/\n", root);

    tree(root, 0);
    return 0;
}


