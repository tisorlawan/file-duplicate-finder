#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

// TODO: glibc compability issues regarding d_type

#define NAMES_DEFAULT_CAP 4

typedef int ErrNo;

typedef struct {
    char **names;
    size_t len;
    size_t cap;
} Names;

Names *names_init()
{
    Names *n = malloc(sizeof(Names));
    n->names = malloc(sizeof(char *) * NAMES_DEFAULT_CAP);
    n->len = 0;
    n->cap = 2;

    return n;
}

void names_add(Names *n, const char *name)
{
    if (n->len == n->cap) {
        n->cap *= 2; // double the capacity
        n->names = realloc(n->names, sizeof(*(n->names)) * n->cap);
    }
    size_t name_len = strlen(name);
    n->names[n->len] = malloc(name_len + 1);
    strncpy(n->names[n->len], name, name_len + 1);
    n->names[n->len][name_len] = '\0';

    n->len += 1;
}

void names_free(Names *n)
{
    for (size_t i = 0; i < n->len; i++) {
        free(n->names[i]);
    }
    free(n->names);
    free(n);
}

void names_debug(Names *n)
{
    for (size_t i = 0; i < n->len; i++) {
        printf("%s\n", n->names[i]);
    }
}

// TODO: handle trailing slash
char *join_path(const char *p1, const char *p2)
{
    size_t p1_len = strlen(p1);
    size_t p2_len = strlen(p2);

    size_t path_len = p1_len + p2_len + 1;
    char *joined_path = malloc(path_len + 1);

    memcpy(joined_path, p1, p1_len);
    joined_path[p1_len] = '/';
    memcpy(joined_path + p1_len + 1, p2, p2_len);
    joined_path[path_len] = '\0';

    return joined_path;
}

ErrNo read_dir(const char *name, Names *dir_names, Names *reg_file_names)
{
    DIR *dir = opendir(name);

    if (dir == NULL) {
        fprintf(stderr, "ERROR: could not open directory '%s': %s\n", name,
                strerror(errno));
        return errno;
    }

    struct dirent *d = NULL;

    do {
        errno = 0;
        d = readdir(dir);
        if (d == NULL) {
            if (errno != 0) {
                fprintf(stderr, "ERROR: could not read directory '%s': %s\n",
                        name, strerror(errno));
                return errno;
            } else {
                // Reached end of the directory stream
            }
        } else {
            size_t dname_len = strlen(d->d_name);
            if (strncmp(d->d_name, ".", dname_len) == 0 ||
                strncmp(d->d_name, "..", dname_len) == 0) {
                continue;
            }
            if (d->d_type == DT_DIR) {
                char *joined_path = join_path(name, d->d_name);

                names_add(dir_names, d->d_name);
                read_dir(joined_path, dir_names, reg_file_names);

                free(joined_path);
            } else if (d->d_type == DT_REG) {
                char *joined_path = join_path(name, d->d_name);

                names_add(reg_file_names, joined_path);

                free(joined_path);
            }
        }
    } while (d != NULL);

    closedir(dir);
    return 0;
}

int main(void)
{
    Names *dir_names = names_init();
    Names *file_names = names_init();

    read_dir("../../..", dir_names, file_names);

    names_debug(file_names);

    names_free(dir_names);
    names_free(file_names);

    return 0;
}
