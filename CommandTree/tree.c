/*
 * tree.c  --  recursive directory listing for Minix
 *
 * Usage: tree [options] [path]
 *
 * Options:
 *   -s        show file size next to each entry
 *   -p        show entry permissions (rwxr-xr-x style)
 *   -t        sort by modification time (newest first)
 *   -d        list directories only
 *   -h        show help
 *
 * System calls used: opendir, readdir, closedir, lstat, readlink
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* ASCII tree drawing characters                                        */
/* ------------------------------------------------------------------ */
#define PIPE   "|   "
#define TEE    "|-- "
#define LAST   "\\-- "
#define BLANK  "    "

#define MAX_PATH   4096
#define MAX_PREFIX 4096

/* ------------------------------------------------------------------ */
/* Global options                                                       */
/* ------------------------------------------------------------------ */
static int opt_size  = 0;   /* -s */
static int opt_perms = 0;   /* -p */
static int opt_time  = 0;   /* -t */
static int opt_dirs  = 0;   /* -d */

/* ------------------------------------------------------------------ */
/* Summary counters                                                     */
/* ------------------------------------------------------------------ */
static int total_dirs  = 0;
static int total_files = 0;

/* ------------------------------------------------------------------ */
/* Entry: name + cached stat (needed for -t sort and -s/-p display)    */
/* ------------------------------------------------------------------ */
typedef struct {
    char        name[256];
    struct stat st;
} Entry;

/* ------------------------------------------------------------------ */
/* qsort comparators                                                    */
/* ------------------------------------------------------------------ */
static int cmp_by_name(const void *a, const void *b)
{
    return strcmp(((const Entry *)a)->name, ((const Entry *)b)->name);
}

static int cmp_by_time(const void *a, const void *b)
{
    time_t ta = ((const Entry *)a)->st.st_mtime;
    time_t tb = ((const Entry *)b)->st.st_mtime;
    /* newest first; fall back to name for ties */
    if (tb != ta)
        return (tb > ta) ? 1 : -1;
    return strcmp(((const Entry *)a)->name, ((const Entry *)b)->name);
}

/* ------------------------------------------------------------------ */
/* Format helpers                                                       */
/* ------------------------------------------------------------------ */
static void fmt_size(off_t sz, char *buf)
{
    if (sz >= 1024 * 1024)
        sprintf(buf, "%5.1fM", sz / (1024.0 * 1024.0));
    else if (sz >= 1024)
        sprintf(buf, "%5.1fK", sz / 1024.0);
    else
        sprintf(buf, "%5ld ", (long)sz);
}

static void fmt_perms(mode_t mode, char *buf)
{
    buf[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

/* ------------------------------------------------------------------ */
/* Recursive tree printer                                               */
/* ------------------------------------------------------------------ */
static void print_tree(const char *path, const char *prefix, int depth)
{
    DIR           *dp;
    struct dirent *ep;
    char           fullpath[MAX_PATH];
    char           newprefix[MAX_PREFIX];
    Entry         *entries;
    int            count, cap, i, is_last, is_dir, is_lnk;

    dp = opendir(path);
    if (!dp) {
        fprintf(stderr, "tree: cannot open '%s'\n", path);
        return;
    }

    cap     = 32;
    count   = 0;
    entries = malloc(cap * sizeof(Entry));
    if (!entries) { closedir(dp); return; }

    while ((ep = readdir(dp)) != NULL) {
        if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
            continue;
        if (ep->d_name[0] == '.')
            continue;

        snprintf(fullpath, MAX_PATH, "%s/%s", path, ep->d_name);
        if (lstat(fullpath, &entries[count].st) < 0)
            continue;

        if (opt_dirs && !S_ISDIR(entries[count].st.st_mode))
            continue;

        strncpy(entries[count].name, ep->d_name, 255);
        entries[count].name[255] = '\0';
        count++;

        if (count == cap) {
            cap    *= 2;
            entries = realloc(entries, cap * sizeof(Entry));
            if (!entries) { closedir(dp); return; }
        }
    }
    closedir(dp);

    qsort(entries, count, sizeof(Entry),
          opt_time ? cmp_by_time : cmp_by_name);

    for (i = 0; i < count; i++) {
        char pbuf[12];   /* "drwxr-xr-x\0" */
        char sbuf[10];   /* "  1.2K\0"     */

        is_last = (i == count - 1);
        is_dir  = S_ISDIR(entries[i].st.st_mode);
        is_lnk  = S_ISLNK(entries[i].st.st_mode);

        printf("%s%s", prefix, is_last ? LAST : TEE);

        if (opt_perms) {
            fmt_perms(entries[i].st.st_mode, pbuf);
            printf("[%s] ", pbuf);
        }
        if (opt_size) {
            fmt_size(entries[i].st.st_size, sbuf);
            printf("[%s] ", sbuf);
        }

        printf("%s", entries[i].name);

        if (is_dir) {
            printf("/\n");
            total_dirs++;
            snprintf(fullpath,   MAX_PATH,   "%s/%s", path, entries[i].name);
            snprintf(newprefix,  MAX_PREFIX, "%s%s",  prefix, is_last ? BLANK : PIPE);
            print_tree(fullpath, newprefix, depth + 1);

        } else if (is_lnk) {
            char lbuf[MAX_PATH];
            int  len;
            snprintf(fullpath, MAX_PATH, "%s/%s", path, entries[i].name);
            len = readlink(fullpath, lbuf, MAX_PATH - 1);
            if (len >= 0) lbuf[len] = '\0';
            printf(" -> %s\n", len >= 0 ? lbuf : "?");
            total_files++;

        } else {
            printf("\n");
            total_files++;
        }
    }

    free(entries);
}

/* ------------------------------------------------------------------ */
/* Usage                                                               */
/* ------------------------------------------------------------------ */
static void usage(const char *prog)
{
    printf("Usage: %s [options] [path]\n\n", prog);
    printf("  Lists directories and files as a tree.\n");
    printf("  Defaults to the current directory if no path is given.\n\n");
    printf("Options:\n");
    printf("  -s        Show size of each entry  (e.g. 1.2K, 340 )\n");
    printf("  -p        Show permissions          (e.g. drwxr-xr-x)\n");
    printf("  -t        Sort by modification time (newest first)\n");
    printf("  -d        List directories only\n");
    printf("  -h        Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s -sp /usr/src\n", prog);
    printf("  %s -t  /etc\n",     prog);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    char *path = ".";
    char *p;
    int   i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            path = argv[i];
            continue;
        }
        for (p = argv[i] + 1; *p; p++) {
            switch (*p) {
            case 's': opt_size  = 1; break;
            case 'p': opt_perms = 1; break;
            case 't': opt_time  = 1; break;
            case 'd': opt_dirs  = 1; break;
            case 'h': usage(argv[0]); return 0;
            default:
                fprintf(stderr, "tree: unknown option '-%c'\n", *p);
                usage(argv[0]);
                return 1;
            }
        }
    }

    printf("%s\n", path);
    print_tree(path, "", 0);

    printf("\n%d director%s, %d file%s\n",
           total_dirs,  total_dirs  == 1 ? "y" : "ies",
           total_files, total_files == 1 ? ""  : "s");

    return 0;
}
