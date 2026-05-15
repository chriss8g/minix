#define _MTHREADIFY_PTHREADS
#include <minix/mthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static const char *errname(int err)
{
    if (err == 0)
    return "OK";
    return strerror(err);
}
int main(void)
{
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int r1, r2, r3, r4;
    r1 = pthread_mutex_trylock(&mutex);
    printf("first trylock: %d (%s)\n", r1, errname(r1));
    r2 = pthread_mutex_trylock(&mutex);
    printf("second trylock: %d (%s)\n", r2, errname(r2));
    r3 = pthread_mutex_unlock(&mutex);
    printf("unlock: %d (%s)\n", r3, errname(r3));
    r4 = pthread_mutex_destroy(&mutex);
    printf("destroy: %d (%s)\n", r4, errname(r4));
    if (r1 != 0)
    return 1;
    if (r2 != EDEADLK && r2 != EBUSY)
    return 2;
    if (r3 != 0)
    return 3;
    if (r4 != 0)
    return 4;
    printf("PASS\n");
    return 0;
}

