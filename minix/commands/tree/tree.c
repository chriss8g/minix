#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>


int files_count;
int dir_count;
int last[1024];

void rtree(char *path,int depth,int parentLast){
    DIR *dir = opendir(path);
    struct dirent *file = readdir(dir);
    struct dirent *next;

    while( file != NULL ) {
        if (file->d_name[0] == '.') {
            file = readdir(dir);
            continue;
        }

        for (int i = 0; i < depth; i++) {
            if(last[i])printf("     ");
            else printf("│    ");
            
        }

        next = readdir(dir);
        if(next==NULL)printf("└─ %s\n", file->d_name);
        else printf("├─ %s\n", file->d_name);

        last[depth]=next==NULL;

        char str[1024];
        strcpy(str, path );
        strcat(str, "/");
        strcat(str, file->d_name );

        struct stat st;
        if (lstat(str, &st) == 0 &&  S_ISDIR(st.st_mode)) {
            dir_count++;
            rtree(str,depth+1,next==NULL);
        }
        else{
            files_count++;
        }

        file=next;
    }
    
    closedir(dir);
}

void tree(char *path){
    files_count=0;
    dir_count=0;

    printf("%s",path);
    printf("\n");

    rtree(path,0,1);

    printf("\n");

    printf("%d directories, %d files\n",dir_count+1,files_count);
}

int main(int argc, char *argv[]) {
    char *path = (argc > 1) ? argv[1] : ".";
    tree(path);
    
    return 0;
}