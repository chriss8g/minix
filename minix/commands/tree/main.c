#include <stdio.h>

void tree(char *path);

int main(int argc, char *argv[]) {
    if (argc > 1)
        tree(argv[1]);
    else
        tree(NULL);

    return 0;
}

