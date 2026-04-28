#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

long TotalSize=0;
long TotalSubdirectories=0;
long TotalFiles=0;

int tree(const char *path, const char *identation); // ← AÑADIDO

int tree(const char *path, const char *identation)
{
    struct dirent *entry;
    struct stat info;
    DIR *dp;

    dp = opendir(path);
    if (dp == NULL)
    {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dp)))
    {
        if (strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0)
            continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        if (stat(fullpath, &info) == -1)
        {
            perror("stat");
            continue;
        }

        if (S_ISDIR(info.st_mode))
        {
            TotalSubdirectories=TotalSubdirectories+1;
            char NewIdentation[1024];
            snprintf(NewIdentation, sizeof(NewIdentation), "%s  ", identation);
            printf("%s/%s\n", identation, entry->d_name);
            tree(fullpath, NewIdentation);
        }
        else
        {
            TotalFiles=TotalFiles+1;
            TotalSize=TotalSize+info.st_size;
            printf("%s%s ---- %lld b\n", identation, entry->d_name, (long long)info.st_size); // ← CAMBIADO
        }
    }

    closedir(dp);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *path; // ← CAMBIADO

    if (argc < 2)
        path = ".";
    else
        path = argv[1];

    printf("Explorando ... [%s]\n", path);
    tree(path, "  ");
    printf("[Total Size: %lld B]\n[Subdirectories: %ld]\n[Files: %ld]\n", (long long)TotalSize, TotalSubdirectories, TotalFiles); // ← CAMBIADO
    return 0;
}