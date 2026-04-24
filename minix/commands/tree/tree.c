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
    struct stat info;
    while ((hs = readdir(directorio)) != NULL)
    {
        if((hs -> d_name[0] == '.' && hs -> d_name[1] == '\0') || (hs -> d_name[0] == '.' && hs -> d_name[1] == '.' && hs -> d_name[2] == '\0'))
        {
            continue;
        }
        printf("%s\n", hs->d_name);
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, hs->d_name);
        lstat(path, &info);
        if(S_ISDIR(info.st_mode))
        {
            tree(path);
        }
        // un if si no es una carpeta solo imprimimos su nombre
        //caso contrario guardamos la direccion de la carpeta la 
        //convertimmos a char y hacemos el llamado recursivo
        
    }
    closedir(directorio);
}

int main(int argc, char *argv[])
{
    // Esto es para el directorio actual
    if(argc == 1)
    {
        tree(".");
    }

    // Esto es para un directorio específico
    else
    {
        tree(argv[1]);
    }
}