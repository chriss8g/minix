#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_PATH 1024
#define MAX_DEPTH 100

/* Funciones auxiliares */
int is_directory(const char *path);
void print_tree(const char *path, int depth, int *total_dirs, int *total_files);
int should_skip(const char *name);

int main(int argc, char *argv[]) {
    char root_path[MAX_PATH];
    int total_dirs = 0, total_files = 0;
    
    /* Determinar la ruta base */
    if (argc < 2) {
        strcpy(root_path, ".");
    } else {
        strcpy(root_path, argv[1]);
    }
    
    /* Imprimir cabecera */
    printf("%s\n", root_path);
    
    /* Generar el árbol */
    print_tree(root_path, 0, &total_dirs, &total_files);
    
    /* Imprimir resumen */
    printf("\n%d directorios, %d archivos\n", total_dirs, total_files);
    
    return 0;
}

/* Verificar si una ruta es un directorio (y no un enlace simbólico) */
int is_directory(const char *path) {
    struct stat st;
    
    if (stat(path, &st) != 0) {
        return 0;
    }
    
    /* S_ISDIR verifica si es directorio */
    /* S_ISLNK - no se siguen enlaces simbólicos */
    return S_ISDIR(st.st_mode);
}

/* Verificar si debemos omitir un archivo/directorio */
int should_skip(const char *name) {
    /* Omitir . y .. */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 1;
    }
    return 0;
}

/* Función principal recursiva para imprimir el árbol */
void print_tree(const char *path, int depth, int *total_dirs, int *total_files) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[MAX_PATH];
    int i;
    
    /* Abrir el directorio actual */
    dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "Error: No se puede abrir '%s': %s\n", 
                path, strerror(errno));
        return;
    }
    
    /* Leer todas las entradas primero para ordenarlas */
    /* (Opcional - Minix puede no tener scandir) */
    struct dirent **entries = NULL;
    int entry_count = 0;
    int entry_capacity = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (should_skip(entry->d_name)) {
            continue;
        }
        
        /* Expandir array dinámicamente */
        if (entry_count >= entry_capacity) {
            entry_capacity = entry_capacity == 0 ? 32 : entry_capacity * 2;
            entries = realloc(entries, entry_capacity * sizeof(struct dirent*));
            if (entries == NULL) {
                closedir(dir);
                return;
            }
        }
        
        /* Copiar la entrada */
        entries[entry_count] = malloc(sizeof(struct dirent));
        if (entries[entry_count] == NULL) {
            closedir(dir);
            return;
        }
        memcpy(entries[entry_count], entry, sizeof(struct dirent));
        entry_count++;
    }
    
    closedir(dir);
    
    /* Procesar cada entrada */
    for (i = 0; i < entry_count; i++) {
        entry = entries[i];
        
        /* Construir ruta completa */
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        /* Obtener información del archivo (sin seguir enlaces simbólicos) */
        if (lstat(full_path, &st) != 0) {
            free(entries[i]);
            continue;
        }
        
        /* Imprimir indentación */
        for (int j = 0; j < depth; j++) {
            printf("|   ");
        }
        
        /* Determinar si es directorio o archivo */
        if (S_ISDIR(st.st_mode)) {
            /* Es directorio - no seguir si es enlace simbólico */
            if (!S_ISLNK(st.st_mode)) {
                printf("|-- %s/\n", entry->d_name);
                (*total_dirs)++;
                
                /* Llamada recursiva para subdirectorio */
                print_tree(full_path, depth + 1, total_dirs, total_files);
            } else {
                printf("|-- %s [enlace simbólico]\n", entry->d_name);
            }
        } else {
            /* Es archivo regular */
            printf("|-- %s\n", entry->d_name);
            (*total_files)++;
        }
        
        free(entries[i]);
    }
    
    free(entries);
}