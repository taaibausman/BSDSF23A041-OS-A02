/* ls-v1.5.0.c
 * Version 1.5.0 — Colorized Output based on file type
 *
 * Features:
 *  - alphabetical sorting (qsort)
 *  - column display (down then across) as default
 *  - horizontal display (-x)
 *  - long listing (-l)
 *  - colorized names:
 *      Directory: blue
 *      Executable: green
 *      Archives (.tar, .gz, .zip, .tgz, .bz2, .xz): red
 *      Symlink: magenta
 *      Special files (device, socket): reverse video
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <stdbool.h>
#include <sys/ioctl.h>   // for terminal size
#include <limits.h>      // for PATH_MAX

/* --- Prototypes --- */
void do_ls(const char *dir);
void do_ls_long(const char *dir);
void do_ls_horizontal(const char *dir);
void print_file_details(const char *path, const char *filename);
void print_permissions(mode_t mode);
int get_terminal_width(void);
int compare_names(const void *a, const void *b);

/* --- ANSI color codes --- */
#define ANSI_RESET      "\033[0m"
#define ANSI_BLUE       "\033[0;34m"  /* directories */
#define ANSI_GREEN      "\033[0;32m"  /* executables */
#define ANSI_RED        "\033[0;31m"  /* archives */
#define ANSI_MAGENTA    "\033[0;35m"  /* symlinks */
#define ANSI_REVERSE    "\033[7m"     /* special files: reverse video */

/* --- Helpers --- */

int get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80; /* fallback */
    return (int)w.ws_col;
}

int compare_names(const void *a, const void *b) {
    const char *n1 = *(const char **)a;
    const char *n2 = *(const char **)b;
    return strcmp(n1, n2);
}

/* Determine whether filename looks like an archive */
static bool is_archive_name(const char *name) {
    if (!name) return false;
    const char *exts[] = { ".tar", ".gz", ".zip", ".tgz", ".bz2", ".xz", NULL };
    for (int i = 0; exts[i] != NULL; ++i) {
        const char *p = strstr(name, exts[i]);
        if (p && (p[0] == '.' || p[-1] != '\0' || 1)) {
            /* ensure extension is at end or followed by nothing:
               strstr is fine because ".tar.gz" will still match and that's OK. */
            /* check that ext occurs at end */
            size_t pos = p - name;
            size_t len = strlen(name);
            if (pos + strlen(exts[i]) == len) return true;
        }
    }
    return false;
}

/* Print a filename with color (but NO padding). Caller will pad as needed. */
static void print_colored_name_no_pad(const char *dir, const char *name) {
    char path[PATH_MAX];
    struct stat st;
    int printed = snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (printed < 0 || (size_t)printed >= sizeof(path)) {
        /* path too long — fallback to name only */
        printf("%s", name);
        return;
    }

    if (lstat(path, &st) == -1) {
        /* Cannot stat — print normally */
        printf("%s", name);
        return;
    }

    /* Choose color/style */
    const char *start = "";
    const char *end = ANSI_RESET;

    if (S_ISLNK(st.st_mode)) {
        start = ANSI_MAGENTA;
    } else if (S_ISDIR(st.st_mode)) {
        start = ANSI_BLUE;
    } else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode)) {
        start = ANSI_REVERSE;
    } else if ((st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0) {
        /* executable bit set for someone */
        start = ANSI_GREEN;
    } else if (is_archive_name(name)) {
        start = ANSI_RED;
    } else {
        start = ""; end = ""; /* no color */
    }

    if (start[0] != '\0')
        printf("%s%s%s", start, name, end);
    else
        printf("%s", name);
}

/* Print a filename *padded* to col_width (visible width is name length).
   We print color sequence + name + reset, then add spaces to reach column width. */
static void print_colored_name_padded(const char *dir, const char *name, int col_width) {
    size_t name_len = strlen(name);
    print_colored_name_no_pad(dir, name);
    int pad = col_width - (int)name_len;
    if (pad < 1) pad = 1; /* at least one space between columns */
    for (int i = 0; i < pad; ++i) putchar(' ');
}

/* --- Display mode enum --- */
enum DisplayMode { DEFAULT, LONG_LIST, HORIZONTAL };

/* --- main --- */
int main(int argc, char *argv[]) {
    int opt;
    enum DisplayMode mode = DEFAULT;

    while ((opt = getopt(argc, argv, "lx")) != -1) {
        switch (opt) {
            case 'l': mode = LONG_LIST; break;
            case 'x': mode = HORIZONTAL; break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind == argc) {
        if (mode == LONG_LIST) do_ls_long(".");
        else if (mode == HORIZONTAL) do_ls_horizontal(".");
        else do_ls(".");
    } else {
        for (int i = optind; i < argc; ++i) {
            printf("Directory listing of %s:\n", argv[i]);
            if (mode == LONG_LIST) do_ls_long(argv[i]);
            else if (mode == HORIZONTAL) do_ls_horizontal(argv[i]);
            else do_ls(argv[i]);
            puts("");
        }
    }

    return 0;
}

/* --- Default (vertical: down then across) --- */
void do_ls(const char *dir) {
    DIR *dp = opendir(dir);
    if (!dp) { perror(dir); return; }

    struct dirent *entry;
    char **files = NULL;
    size_t count = 0, max_len = 0;

    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* skip hidden */
        char *dup = strdup(entry->d_name);
        if (!dup) continue;
        char **tmp = realloc(files, (count + 1) * sizeof(char*));
        if (!tmp) { free(dup); break; }
        files = tmp;
        files[count++] = dup;
        size_t L = strlen(dup);
        if (L > max_len) max_len = L;
    }
    closedir(dp);

    if (count == 0) {
        free(files);
        return;
    }

    /* Sort alphabetically */
    qsort(files, count, sizeof(char*), compare_names);

    int term_width = get_terminal_width();
    int spacing = 2;
    int col_width = (int)max_len + spacing;
    if (col_width < 1) col_width = 1;
    int cols = term_width / col_width;
    if (cols < 1) cols = 1;
    int rows = (int)((count + cols - 1) / cols); /* ceil */

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int idx = c * rows + r;
            if ((size_t)idx >= count) continue;
            print_colored_name_padded(dir, files[idx], col_width);
        }
        putchar('\n');
    }

    for (size_t i = 0; i < count; ++i) free(files[i]);
    free(files);
}

/* --- Horizontal display (left-to-right) --- */
void do_ls_horizontal(const char *dir) {
    DIR *dp = opendir(dir);
    if (!dp) { perror(dir); return; }

    struct dirent *entry;
    char **files = NULL;
    size_t count = 0, max_len = 0;

    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char *dup = strdup(entry->d_name);
        if (!dup) continue;
        char **tmp = realloc(files, (count + 1) * sizeof(char*));
        if (!tmp) { free(dup); break; }
        files = tmp;
        files[count++] = dup;
        size_t L = strlen(dup);
        if (L > max_len) max_len = L;
    }
    closedir(dp);

    if (count == 0) { free(files); return; }

    /* Sort alphabetically */
    qsort(files, count, sizeof(char*), compare_names);

    int term_width = get_terminal_width();
    int spacing = 2;
    int col_width = (int)max_len + spacing;
    if (col_width < 1) col_width = 1;

    int current = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t name_len = strlen(files[i]);
        /* if next would overflow, newline */
        if (current + col_width > term_width) {
            putchar('\n');
            current = 0;
        }
        print_colored_name_no_pad(dir, files[i]);
        /* pad visibly */
        int pad = col_width - (int)name_len;
        if (pad < 1) pad = 1;
        for (int p = 0; p < pad; ++p) putchar(' ');
        current += col_width;
    }
    putchar('\n');

    for (size_t i = 0; i < count; ++i) free(files[i]);
    free(files);
}

/* --- Long listing (-l) --- */
void do_ls_long(const char *dir) {
    DIR *dp = opendir(dir);
    if (!dp) { perror(dir); return; }

    struct dirent *entry;
    char **files = NULL;
    size_t count = 0;

    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char *dup = strdup(entry->d_name);
        if (!dup) continue;
        char **tmp = realloc(files, (count + 1) * sizeof(char*));
        if (!tmp) { free(dup); break; }
        files = tmp;
        files[count++] = dup;
    }
    closedir(dp);

    if (count == 0) { free(files); return; }

    qsort(files, count, sizeof(char*), compare_names);

    for (size_t i = 0; i < count; ++i) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        print_file_details(path, files[i]);
        free(files[i]);
    }
    free(files);
}

/* --- print metadata for -l --- */
void print_file_details(const char *path, const char *filename) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        perror("stat");
        return;
    }

    print_permissions(st.st_mode);
    printf(" %2ld", (long)st.st_nlink);

    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    printf(" %-8s %-8s", pw ? pw->pw_name : "unknown", gr ? gr->gr_name : "unknown");

    printf(" %8ld", (long)st.st_size);

    char timebuf[64];
    struct tm *t = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", t);
    printf(" %s ", timebuf);

    /* finally print colorized name and newline */
    print_colored_name_no_pad(".", filename); /* path already printed, but color needs file path; print using filename alone */
    printf("\n");
}

/* --- permission printing --- */
void print_permissions(mode_t mode) {
    char perms[11];
    perms[0] = S_ISDIR(mode) ? 'd' :
               S_ISLNK(mode) ? 'l' :
               S_ISCHR(mode) ? 'c' :
               S_ISBLK(mode) ? 'b' :
               S_ISSOCK(mode) ? 's' :
               S_ISFIFO(mode) ? 'p' : '-';

    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';

    printf("%s", perms);
}
