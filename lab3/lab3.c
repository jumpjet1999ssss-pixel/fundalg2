#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <stdint.h>

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_NULL,
    ERR_MEMORY,
    ERR_OPEN,
    ERR_READ,
    ERR_EMPTY_SUBSTR
} Status;

static const char *status_msg(Status s) {
    switch (s) {
    case OK: return "ok";
    case ERR_ARGS: return "invalid arguments";
    case ERR_NULL: return "null pointer";
    case ERR_MEMORY: return "memory allocation failed";
    case ERR_OPEN: return "cannot open file";
    case ERR_READ: return "file read error";
    case ERR_EMPTY_SUBSTR: return "empty substring";
    default: return "unknown error";
    }
}

static Status load_file(const char *path, char **out_buf, size_t *out_len) {
    if (!path || !out_buf || !out_len) return ERR_NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return ERR_OPEN;

    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap * sizeof(char));
    if (!buf) { fclose(f); return ERR_MEMORY; }

    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 1 >= cap) {
            if (cap > SIZE_MAX / 2) {
                free(buf);
                fclose(f);
                return ERR_MEMORY;
            }
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap * sizeof(char));
            if (!tmp) { 
                free(buf); 
                fclose(f); 
                return ERR_MEMORY; 
            }
            buf = tmp;
        }
        buf[len++] = (char)c;
    }

    if (ferror(f)) {
        free(buf);
        fclose(f);
        return ERR_READ;
    }
    
    fclose(f);
    buf[len] = '\0'; 

    *out_buf = buf;
    *out_len = len;
    return OK;
}


typedef struct {
    size_t line;
    size_t column;
} Match;

typedef struct {
    const char *path;
    Match *matches;
    size_t count;
} FileMatches;

static void free_results(FileMatches *results, size_t count) {
    if (!results) return;
    for (size_t i = 0; i < count; i++) free(results[i].matches);
    free(results);
}

static Status find_occurrences(const char *text, size_t text_len,
                              const char *pat, size_t pat_len,
                              Match **out_matches, size_t *out_count) {
    if (!text || !pat || !out_matches || !out_count) return ERR_NULL;
    if (pat_len == 0) return ERR_EMPTY_SUBSTR;
    *out_matches = NULL;
    *out_count = 0;
    if (text_len < pat_len) return OK;

    size_t shift[UCHAR_MAX + 1];
    for (size_t c = 0; c <= UCHAR_MAX; c++) shift[c] = pat_len;
    for (size_t j = 0; j + 1 < pat_len; j++)
        shift[(unsigned char)pat[j]] = pat_len - 1 - j;

    Match *matches = NULL;
    size_t count = 0, capacity = 0;
    size_t cursor = 0, line = 1, column = 1;
    size_t i = 0;
    while (i <= text_len - pat_len) {
        size_t j = pat_len;
        while (j > 0 && text[i + j - 1] == pat[j - 1]) j--;
        if (j == 0) {
            if (count == capacity) {
                size_t next = capacity ? capacity * 2 : 8;
                if (next < capacity || next > SIZE_MAX / sizeof(Match)) {
                    free(matches);
                    return ERR_MEMORY;
                }
                Match *tmp = realloc(matches, next * sizeof(Match));
                if (!tmp) { free(matches); return ERR_MEMORY; }
                matches = tmp;
                capacity = next;
            }
            // Учитываем также символы, пропущенные сдвигом алгоритма поиска.
            while (cursor < i) {
                if (text[cursor++] == '\n') { line++; column = 1; }
                else column++;
            }
            matches[count++] = (Match){line, column};
        }
        size_t step = shift[(unsigned char)text[i + pat_len - 1]];
        if (step > text_len - pat_len - i) break;
        i += step;
    }
    *out_matches = matches;
    *out_count = count;
    return OK;
}

static Status search_paths(const char *substr, int nfiles,
                           const char *const *paths, FileMatches **out_results) {
    if (!substr || !out_results || !paths) return ERR_NULL;
    *out_results = NULL;
    if (nfiles <= 0) return ERR_ARGS;
    size_t length = strlen(substr);
    if (length == 0) return ERR_EMPTY_SUBSTR;
    if ((size_t)nfiles > SIZE_MAX / sizeof(FileMatches)) return ERR_MEMORY;
    FileMatches *results = calloc((size_t)nfiles, sizeof(FileMatches));
    if (!results) return ERR_MEMORY;

    for (int i = 0; i < nfiles; i++) {
        char *text = NULL;
        size_t text_len = 0;
        Status st = load_file(paths[i], &text, &text_len);
        if (st == OK) {
            results[i].path = paths[i];
            st = find_occurrences(text, text_len, substr, length,
                                  &results[i].matches, &results[i].count);
        }
        free(text);
        if (st != OK) {
            free_results(results, (size_t)nfiles);
            return st;
        }
    }
    *out_results = results;
    return OK;
}

Status search_in_files(const char *substr, int nfiles, FileMatches **out_results, ...) {
    if (!out_results) return ERR_NULL;
    *out_results = NULL;
    if (!substr) return ERR_NULL;
    if (nfiles <= 0) return ERR_ARGS;
    if ((size_t)nfiles > SIZE_MAX / sizeof(const char *)) return ERR_MEMORY;
    const char **paths = malloc((size_t)nfiles * sizeof(const char *));
    if (!paths) return ERR_MEMORY;
    va_list ap;
    va_start(ap, out_results);
    for (int i = 0; i < nfiles; i++) paths[i] = va_arg(ap, const char *);
    va_end(ap);
    Status st = search_paths(substr, nfiles, paths, out_results);
    free(paths);
    return st;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <substring> <file> [<file>...]\n", argv[0]);
        return 1;
    }
    FileMatches *results = NULL;
    int count = argc - 2;
    Status st;
    // Демонстрация функции с переменным числом аргументов.
    if (count == 1) st = search_in_files(argv[1], 1, &results, (const char *)argv[2]);
    else if (count == 2)
        st = search_in_files(argv[1], 2, &results, (const char *)argv[2], (const char *)argv[3]);
    else st = search_paths(argv[1], count, (const char *const *)(argv + 2), &results);
    if (st != OK) {
        fprintf(stderr, "Error: %s\n", status_msg(st));
        return 1;
    }
    for (int i = 0; i < count; i++) {
        printf("%s: %zu matches\n", results[i].path, results[i].count);
        for (size_t j = 0; j < results[i].count; j++)
            printf("  line %zu, column %zu\n", results[i].matches[j].line,
                   results[i].matches[j].column);
    }
    free_results(results, (size_t)count);
    return 0;
}
