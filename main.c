#include <limits.h>
#include <stddef.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

// TODO: glibc compability issues regarding d_type

#define NAMES_DEFAULT_CAP 4
#define BUCKET_SIZE 1024 * 1024 * 10

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

unsigned int hash(unsigned int x, size_t bucket_size)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x % bucket_size;
}

unsigned long djb2_hash(char *str, size_t n, size_t bucket_size)
{
    unsigned long hash = 5381;
    int c;
    int m = 0;
    while ((c = *str++) && m < (int)n) {
        m += 1;
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash % bucket_size;
}

typedef struct {
    Names **names;
    size_t bucket_size;
} NameBucket;

NameBucket *name_bucket_init(size_t bucket_size)
{
    NameBucket *nb = malloc(sizeof(NameBucket));
    nb->names = calloc(bucket_size, sizeof(Names *));
    nb->bucket_size = bucket_size;

    for (size_t i = 0; i < bucket_size; i++) {
        nb->names[i] = NULL;
    }

    if (nb->names == NULL) {
        printf("ERROR: %s\n", strerror(errno));
    }
    return nb;
}

void name_bucket_add_entry_size(NameBucket *nb, const char *name, size_t size)
{
    unsigned int h = hash(size, nb->bucket_size);
    if (nb->names[h] == NULL) {
        nb->names[h] = names_init();
    }
    names_add(nb->names[h], name);
}

void name_bucket_add_entry_chars(NameBucket *nb, const char *name, char *chars,
                                 size_t n)
{
    unsigned int h = djb2_hash(chars, n, nb->bucket_size);
    if (nb->names[h] == NULL) {
        nb->names[h] = names_init();
    }
    names_add(nb->names[h], name);
}

void name_bucket_debug(NameBucket *nb)
{
    int multi_files_bucket = 0;
    int total_bucket = 0;

    for (size_t i = 0; i < nb->bucket_size; i++) {
        if (nb->names[i] != NULL) {
            if (nb->names[i]->len > 1) {
                printf("Bucket: %zu\n", i);
                multi_files_bucket += 1;
                names_debug(nb->names[i]);
            }
            total_bucket += 1;
        }
    }
    if (multi_files_bucket > 0) {
        printf("Number of multi files bucket: %d\n", multi_files_bucket);
        printf("Number of bucket: %d\n", total_bucket);
    }
}

void name_bucket_free(NameBucket *nb)
{
    for (size_t i = 0; i < nb->bucket_size; i++) {
        if (nb->names[i] != NULL) {
            names_free(nb->names[i]);
        }
    }
    free(nb->names);
    free(nb);
}

void stat_files(Names *n, NameBucket *nb, size_t min_size)
{
    struct stat *s = malloc(sizeof(struct stat));
    for (size_t i = 0; i < n->len; i++) {
        stat(n->names[i], s);
        if ((size_t)s->st_size > min_size) {
            name_bucket_add_entry_size(nb, n->names[i], s->st_size);
        }
        /* printf("[%zu] %s\n", s->st_size, n->names[i]); */
    }
    free(s);
}

int main(void)
{
    NameBucket *nb = name_bucket_init(BUCKET_SIZE);

    Names *dir_names = names_init();
    Names *file_names = names_init();

    read_dir("tests", dir_names, file_names);

    stat_files(file_names, nb, 1024);

    for (size_t b_idx = 0; b_idx < nb->bucket_size; b_idx++) {
        if (nb->names[b_idx] != NULL && nb->names[b_idx]->len > 1) {
            /* printf("\nChecking bucket [%zu] of %zu\n", b_idx, */
            /*        nb->names[b_idx]->len); */

            size_t nf = nb->names[b_idx]->len;

            FILE **files = malloc(nf * sizeof(FILE *));
            for (size_t j = 0; j < nf; j++) {
                files[j] = fopen(nb->names[b_idx]->names[j], "rb");
            }

            int *checked = malloc(nf * sizeof(int));
            for (size_t j = 0; j < nf; j++) {
                checked[j] = 1;
            }

            int initial_bs = 1024 * 1024 * 10;
            char *buffer = malloc(initial_bs);
            NameBucket *nb_inner = NULL;

            int done = 0;
            for (size_t bs = initial_bs; done != 1; bs *= 2) {
                buffer = realloc(buffer, bs);

                nb_inner = name_bucket_init(nf * 512);

                int checked_num = 0;
                for (size_t j = 0; j < nf; j++) {
                    checked_num += checked[j];
                }
                if (checked_num == 0) {
                    done = 1;
                }

                if (done == 0) {
                    for (size_t file_idx = 0; file_idx < nf; file_idx++) {
                        if (checked[file_idx] == 0) {
                            continue;
                        }
                        memset(buffer, 0, bs);

                        size_t n = fread(buffer, sizeof(char), bs,
                                         files[file_idx]);
                        if (n > 0) {
                            /* printf("x %s\n", nb->names[b_idx]->names[file_idx]); */
                            name_bucket_add_entry_chars(
                                    nb_inner, nb->names[b_idx]->names[file_idx],
                                    buffer, n);
                        }
                        if (feof(files[file_idx])) {
                            done = 1;
                        }
                    }
                }

                if (done == 1) {
                    printf("\n");
                    name_bucket_debug(nb_inner);
                    name_bucket_free(nb_inner);
                    continue;
                }

                for (size_t m = 0; m < nf; m++) {
                    checked[m] = 0;
                }

                for (size_t k = 0; k < nb_inner->bucket_size; k++) {
                    if (nb_inner->names[k] != NULL &&
                        nb_inner->names[k]->len > 1) {
                        for (size_t l = 0; l < nb_inner->names[k]->len; l++) {
                            for (size_t n = 0; n < nf; n++) {
                                if (strcmp(nb_inner->names[k]->names[l],
                                           nb->names[b_idx]->names[n]) == 0) {
                                    checked[n] = 1;
                                    break;
                                }
                            }
                        }
                    } else if (nb_inner->names[k] != NULL &&
                               nb_inner->names[k]->len == 1) {
                        for (size_t n = 0; n < nf; n++) {
                            if (strcmp(nb_inner->names[k]->names[0],
                                       nb->names[b_idx]->names[n]) == 0) {
                                checked[n] = 0;
                                break;
                            }
                        }
                    }
                }
                name_bucket_free(nb_inner);
            }

            for (size_t j = 0; j < nf; j++) {
                if (files[j] != NULL) {
                    fclose(files[j]);
                }
            }
            free(files);

            free(checked);
            free(buffer);
        }
    }

    names_free(dir_names);
    names_free(file_names);

    name_bucket_free(nb);

    return 0;
}
