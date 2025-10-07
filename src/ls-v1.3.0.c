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

// Function declarations
void do_ls(const char *dir);
void do_ls_long(const char *dir);
void do_ls_horizontal(const char *dir);
void print_file_details(const char *path, const char *filename);
void print_permissions(mode_t mode);

// Helper for terminal width
int get_terminal_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80; // fallback width
    return w.ws_col;
}

// Enum to track display mode
enum DisplayMode { DEFAULT, LONG_LIST, HORIZONTAL };

int main(int argc, char *argv[])
{
    int opt;
    enum DisplayMode mode = DEFAULT;

    // Parse command line options (-l and -x)
    while ((opt = getopt(argc, argv, "lx")) != -1) {
        switch (opt) {
            case 'l':
                mode = LONG_LIST;
                break;
            case 'x':
                mode = HORIZONTAL;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // No directory specified → use current
    if (optind == argc) {
        if (mode == LONG_LIST)
            do_ls_long(".");
        else if (mode == HORIZONTAL)
            do_ls_horizontal(".");
        else
            do_ls(".");
    } else {
        for (int i = optind; i < argc; i++) {
            printf("Directory listing of %s:\n", argv[i]);
            if (mode == LONG_LIST)
                do_ls_long(argv[i]);
            else if (mode == HORIZONTAL)
                do_ls_horizontal(argv[i]);
            else
                do_ls(argv[i]);
            puts("");
        }
    }

    return 0;
}

// Default display: down-then-across column layout
void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (!dp) {
        perror(dir);
        return;
    }

    char **files = NULL;
    size_t count = 0, max_len = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        files = realloc(files, sizeof(char*) * (count + 1));
        files[count] = strdup(entry->d_name);
        if (strlen(entry->d_name) > max_len)
            max_len = strlen(entry->d_name);
        count++;
    }
    closedir(dp);

    if (count == 0) return;

    int term_width = get_terminal_width();
    int spacing = 2;
    int col_width = max_len + spacing;
    int cols = term_width / col_width;
    if (cols == 0) cols = 1;
    int rows = (count + cols - 1) / cols;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int idx = c * rows + r;
            if (idx >= count) continue;
            printf("%-*s", col_width, files[idx]);
        }
        printf("\n");
    }

    for (size_t i = 0; i < count; i++)
        free(files[i]);
    free(files);
}

// New: Horizontal (left-to-right) display for -x flag
void do_ls_horizontal(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (!dp) {
        perror(dir);
        return;
    }

    char **files = NULL;
    size_t count = 0, max_len = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        files = realloc(files, sizeof(char*) * (count + 1));
        files[count] = strdup(entry->d_name);
        if (strlen(entry->d_name) > max_len)
            max_len = strlen(entry->d_name);
        count++;
    }
    closedir(dp);

    if (count == 0) return;

    int term_width = get_terminal_width();
    int spacing = 2;
    int col_width = max_len + spacing;

    int current_pos = 0;
    for (size_t i = 0; i < count; i++) {
        int next_pos = current_pos + col_width;
        if (next_pos > term_width) {
            printf("\n");
            current_pos = 0;
        }
        printf("%-*s", col_width, files[i]);
        current_pos += col_width;
    }
    printf("\n");

    for (size_t i = 0; i < count; i++)
        free(files[i]);
    free(files);
}

// Long listing (unchanged)
void do_ls_long(const char *dir) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (!dp) {
        perror(dir);
        return;
    }

    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        print_file_details(path, entry->d_name);
    }

    closedir(dp);
}

void print_file_details(const char *path, const char *filename) {
    struct stat info;
    if (lstat(path, &info) == -1) {
        perror("stat");
        return;
    }

    print_permissions(info.st_mode);
    printf(" %2ld", info.st_nlink);

    struct passwd *pw = getpwuid(info.st_uid);
    struct group *gr = getgrgid(info.st_gid);
    printf(" %-8s %-8s", pw ? pw->pw_name : "unknown", gr ? gr->gr_name : "unknown");

    printf(" %8ld", (long)info.st_size);

    char timebuf[64];
    struct tm *timeinfo = localtime(&info.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", timeinfo);
    printf(" %s", timebuf);

    printf(" %s\n", filename);
}
void print_horizontal(char **names, int count, int max_name_len, int term_width) {
    int col_width = max_name_len + 2;  // space padding between columns
    int current_width = 0;

    for (int i = 0; i < count; i++) {
        int len = strlen(names[i]);

        // If next column exceeds terminal width, go to next line
        if (current_width + col_width > term_width) {
            printf("\n");
            current_width = 0;
        }

        // Print filename
        printf("%-*s", col_width, names[i]);  // left-align with padding
        current_width += col_width;
    }

    printf("\n");  // final newline
}

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
