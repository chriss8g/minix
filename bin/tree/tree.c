#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

struct File
{
    char *name;
};

struct Folder
{
    char *name;
    int cntFolders;
    struct Folder **folders;
    int cntFiles;
    struct File **files;
    int permission;
};

int cmp_folder(const void *a, const void *b)
{
    return strcmp((*(struct Folder **)a)->name, (*(struct Folder **)b)->name);
}

int cmp_file(const void *a, const void *b)
{
    return strcmp((*(struct File **)a)->name, (*(struct File **)b)->name);
}

int BuildFolder(struct Folder *folder, char *path, char *name)
{
    folder->cntFiles = 0;
    folder->cntFolders = 0;
    folder->files = NULL;
    folder->folders = NULL;
    folder->permission = 1;
    folder->name = malloc(strlen(name) + 1);
    strcpy(folder->name, name);

    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        folder->permission = 0;
        return 2;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        int newPathLen = strlen(path) + 1 + strlen(entry->d_name) + 1;
        char *newPath = malloc(newPathLen);
        if (newPath == NULL)
        {
            perror("malloc");
            continue;
        }

        snprintf(newPath, newPathLen, "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (lstat(newPath, &statbuf) == -1)
        {
            free(newPath);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            folder->cntFolders++;
        }
        else
            folder->cntFiles++;
        free(newPath);
    }

    closedir(dir);

    /*********************************** 88*********************************************************/

    dir = opendir(path);
    folder->folders = malloc(sizeof(struct Folder *) * folder->cntFolders);
    if (folder->folders == NULL)
    {
        perror("malloc\n");
        return 1;
    }
    folder->files = malloc(sizeof(struct File *) * (folder->cntFiles));
    if (folder->files == NULL)
    {
        perror("malloc\n");
        return 1;
    }

    int fo = 0;
    int fi = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        int newPathLen = strlen(path) + 1 + strlen(entry->d_name) + 1;
        char *newPath = malloc(newPathLen);
        if (newPath == NULL)
        {
            perror("malloc");
            continue;
        }

        snprintf(newPath, newPathLen, "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (lstat(newPath, &statbuf) == -1)
        {
            free(newPath);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            folder->folders[fo] = malloc(sizeof(struct Folder));
            fo++;
            int ressult = 0;
            ressult = BuildFolder(folder->folders[fo - 1], newPath, entry->d_name);
            if (ressult == 1)
                return ressult;
            if (ressult == 2)
                folder->folders[fo - 1]->permission = 0;
        }
        else
        {
            folder->files[fi] = malloc(sizeof(struct File));
            folder->files[fi]->name = malloc(strlen(entry->d_name) + 1);
            strcpy(folder->files[fi]->name, entry->d_name);
            fi++;
        }
        free(newPath);
    }

    closedir(dir);

    qsort(folder->folders, folder->cntFolders, sizeof(struct Folder *), cmp_folder);
    qsort(folder->files, folder->cntFiles, sizeof(struct File *), cmp_file);

    return 0;
}
void PrintTree(struct Folder *folder, int off, int es_ultimo, char *prefijo)
{
    printf("%s", prefijo);

    if (off > 0)
    {
        if (es_ultimo)
            printf("└── ");
        else
            printf("├── ");
    }

    printf("%s", folder->name);
    if (folder->permission == 0)
    {
        printf(" [no hay permisos para entrar]\n");
        return;
    }
    else
        printf("\n");

    char nuevo_prefijo[1024];
    if (off == 0)
        snprintf(nuevo_prefijo, sizeof(nuevo_prefijo), "    ");
    else if (es_ultimo)
        snprintf(nuevo_prefijo, sizeof(nuevo_prefijo), "%s    ", prefijo);
    else
        snprintf(nuevo_prefijo, sizeof(nuevo_prefijo), "%s│   ", prefijo);

    int total = 0;
    for (int i = 0; i < folder->cntFolders; i++)
        if (folder->folders[i]->name[0] != '.')
            total++;
    for (int i = 0; i < folder->cntFiles; i++)
        if (folder->files[i]->name[0] != '.')
            total++;

    int actual = 0;

    for (int i = 0; i < folder->cntFolders; i++)
    {
        if (folder->folders[i]->name[0] == '.')
            continue;
        actual++;
        PrintTree(folder->folders[i], off + 4, actual == total, nuevo_prefijo);
    }

    for (int i = 0; i < folder->cntFiles; i++)
    {
        if (folder->files[i]->name[0] == '.')
            continue;
        actual++;

        printf("%s", nuevo_prefijo);
        if (actual == total)
            printf("└── ");
        else
            printf("├── ");

        printf("%s\n", folder->files[i]->name);
    }
}

void Free(struct Folder *folder)
{
    free(folder->name);

    for (int i = 0; i < folder->cntFolders; i++)
        Free(folder->folders[i]);
    for (int i = 0; i < folder->cntFiles; i++)
    {
        free(folder->files[i]->name);
        free(folder->files[i]);
    }

    free(folder->files);
    free(folder->folders);
}

int main(int argc, char *argv[])
{
    char *path;
    if (argc == 1)
        path = ".";
    else
        path = argv[1];

    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        printf("La ruta <<%s>> no se puede abrir.\n", path);
        return 1;
    }

    struct stat statbuf;
    if (lstat(path, &statbuf) == -1)
    {
        perror("error lstat\n");
        return 1;
    }

    if (!(S_ISDIR(statbuf.st_mode)))
    {
        printf("La ruta que pasaste no es un directorio\n");
        return 0;
    }

    closedir(dir);

    struct Folder *root = malloc(sizeof(struct Folder));
    if (root == NULL)
    {
        perror("malloc\n");
        return 1;
    }
    int ressult = BuildFolder(root, path, path);
    if (ressult == 0)
        PrintTree(root, 0, 1, "");
    else if (ressult == 2)
        printf("No tienes permiso para abrir la ruta: %s\n", path);
    else
    {
        printf("error\n");
        return 1;
    }

    Free(root);
    free(root);

    return 0;
}