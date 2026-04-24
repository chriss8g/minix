#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

void tree(char *dir)
{
    DIR *directorio = opendir(dir);

    if(directorio == NULL)
    {
        perror("Error al abrir el directorio");
        return;
    }

     struct dirent *hs;

    while ((hs = readdir(directorio)) != NULL)
    {
        if((hs -> d_name[0] == '.' && hs -> d_name[1] == '\0') || (hs -> d_name[0] == '.' && hs -> d_name[1] == '.' && hs -> d_name[2] == '\0'))
        {
            continue;
        }
        printf("%s\n", hs->d_name);
    }
    closedir(directorio);
}

int main(int argc, char *argv[])
{
    // Esto es para el directorio actual
    if(argc == 1)
    {
        tree(NULL);
    }

    // Esto es para un directorio específico
    else
    {
        tree(argv[1]);
    }
}