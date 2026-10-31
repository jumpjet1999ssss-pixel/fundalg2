#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS,
    ERR_MEMORY_ALLOCATION,
    ERR_INVALID_FORMAT,
    ERR_CAPACITY_TOO_LARGE,
    ERR_READ
} status_t;

typedef struct {
    char *name;
    unsigned int size;
    unsigned int importance;
} FileInfo;

typedef struct {
    FileInfo *files;
    size_t count;
    unsigned long long total_size;
    unsigned long long total_importance;
} OptimalSet;

static const char *status_msg(status_t s) {
    switch (s) {
    case SUCCESS: return "success";
    case ERR_INVALID_ARGUMENTS: return "invalid arguments";
    case ERR_MEMORY_ALLOCATION: return "memory allocation failed";
    case ERR_INVALID_FORMAT: return "invalid input format";
    case ERR_CAPACITY_TOO_LARGE: return "capacity is too large for memory";
    case ERR_READ: return "read error";
    default: return "unknown error";
    }
}

// парсинг
static status_t read_line(FILE *f, char **out_line) {
    if (f == NULL || out_line == NULL) return ERR_INVALID_ARGUMENTS;
    
    size_t cap = 64, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return ERR_MEMORY_ALLOCATION;

    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            if (cap > SIZE_MAX / 2) {
                free(buf);
                return ERR_MEMORY_ALLOCATION;
            }
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) { free(buf); return ERR_MEMORY_ALLOCATION; }
            buf = tmp;
        }
        buf[len++] = (char)c;
    }
    
    if (ferror(f)) {
        free(buf);
        return ERR_READ;
    }

    if (c == EOF && len == 0) {
        free(buf);
        *out_line = NULL;
        return SUCCESS;
    }
    
    if (len > 0 && buf[len - 1] == '\r') len--;
    buf[len] = '\0';
    *out_line = buf;
    return SUCCESS;
}

static status_t parse_unsigned_int(const char **p, unsigned int *out) {
    if (!p || !*p || !out) return ERR_INVALID_ARGUMENTS;
    if (**p < '0' || **p > '9') return ERR_INVALID_FORMAT;
    unsigned long long val = 0;
    while (**p >= '0' && **p <= '9') {
        val = val * 10ULL + (unsigned long long)(**p - '0');
        if (val > (unsigned long long)UINT_MAX) return ERR_INVALID_FORMAT;
        (*p)++;
    }
    *out = (unsigned int)val;
    return SUCCESS;
}

static status_t parse_line(const char *line, FileInfo *out) {
    if (!line || !out) return ERR_INVALID_ARGUMENTS;
    const char *p = line;
    while (isspace((unsigned char)*p)) p++;

    const char *name_start = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) p++;
    if (p == name_start) return ERR_INVALID_FORMAT;

    size_t name_len = (size_t)(p - name_start);
    out->name = (char *)malloc(name_len + 1);
    if (!out->name) return ERR_MEMORY_ALLOCATION;
    memcpy(out->name, name_start, name_len);
    out->name[name_len] = '\0';

    while (isspace((unsigned char)*p)) p++;
    status_t st = parse_unsigned_int(&p, &out->size);
    if (st != SUCCESS || out->size == 0) return ERR_INVALID_FORMAT;

    if (!isspace((unsigned char)*p)) return ERR_INVALID_FORMAT;
    while (isspace((unsigned char)*p)) p++;
    st = parse_unsigned_int(&p, &out->importance);
    if (st != SUCCESS) return ERR_INVALID_FORMAT;

    while (isspace((unsigned char)*p)) p++;
    if (*p != '\0') return ERR_INVALID_FORMAT;

    return SUCCESS;
}

// основной алгоритм
static status_t find_optimal_set(const FileInfo *files, size_t n, unsigned int capacity, OptimalSet *out_plan) {
    if (!files || !out_plan) return ERR_INVALID_ARGUMENTS;
    if (n == SIZE_MAX) return ERR_CAPACITY_TOO_LARGE;

    out_plan->files = NULL;
    out_plan->count = 0;
    out_plan->total_size = 0;
    out_plan->total_importance = 0;

    if (n == 0 || capacity == 0) return SUCCESS;

    // создаем 2д таблицу
    size_t width = (size_t)capacity + 1;
    
    if (width > SIZE_MAX / (n + 1) / sizeof(unsigned long long)) {
        return ERR_CAPACITY_TOO_LARGE;
    }
    size_t total_cells = (n + 1) * width;

    unsigned long long *dp = (unsigned long long *)calloc(total_cells, sizeof(unsigned long long));
    if (!dp) return ERR_MEMORY_ALLOCATION;

    // заполняем ее
    for (size_t k = 1; k <= n; k++) {
        unsigned int w_k = files[k - 1].size;
        unsigned int p_k = files[k - 1].importance;

        for (size_t s = 0; s <= capacity; s++) {
            unsigned long long not_take = dp[(k - 1) * width + s];
            
            unsigned long long take = 0;
            if (s >= w_k) {
                take = dp[(k - 1) * width + (s - w_k)] + p_k;
            }

            if (take > not_take) {
                dp[k * width + s] = take;
            } else {
                dp[k * width + s] = not_take;
            }
        }
    }

    size_t selected_count = 0;
    size_t s = capacity;

    for (size_t k = n; k >= 1; k--) {
        if (dp[k * width + s] != dp[(k - 1) * width + s]) {
            s -= files[k - 1].size;
            selected_count++;
        }
    }

    if (selected_count > 0) {
        out_plan->files = (FileInfo *)malloc(selected_count * sizeof(FileInfo));
        if (!out_plan->files) { 
            free(dp); 
            return ERR_MEMORY_ALLOCATION; 
        }
        
        out_plan->count = selected_count;
        size_t pos = selected_count;
        s = capacity;

        for (size_t k = n; k >= 1; k--) {
            if (dp[k * width + s] != dp[(k - 1) * width + s]) {
                size_t idx = k - 1;
                s -= files[idx].size;
                pos--;
                
                out_plan->files[pos] = files[idx];
                out_plan->total_size += files[idx].size;
                out_plan->total_importance += files[idx].importance;
            }
        }
    }

    free(dp);
    return SUCCESS;
}

int main(void) {
    char *line = NULL;
    
    if (read_line(stdin, &line) != SUCCESS || !line) {
        fprintf(stderr, "Error: empty input\n");
        return 1;
    }

    const char *p = line;
    unsigned int n, capacity;
    if (parse_unsigned_int(&p, &n) != SUCCESS) { free(line); fprintf(stderr, "Error: invalid n\n"); return 1; }
    while (isspace((unsigned char)*p)) p++;
    if (parse_unsigned_int(&p, &capacity) != SUCCESS) { free(line); fprintf(stderr, "Error: invalid capacity\n"); return 1; }
    
    while (isspace((unsigned char)*p)) p++;
    if (*p != '\0') { free(line); fprintf(stderr, "Error: garbage after header\n"); return 1; }
    free(line);

    if (n == 0) {
        fprintf(stderr, "Error: n must be positive\n");
        return 1;
    }

    FileInfo *files = (FileInfo *)calloc(n, sizeof(FileInfo));
    if (!files) { fprintf(stderr, "Error: memory\n"); return 1; }

    size_t read_count = 0;
    for (unsigned int i = 0; i < n; i++) {
        if (read_line(stdin, &line) != SUCCESS || !line) {
            fprintf(stderr, "Error: unexpected EOF\n");
            for (size_t j = 0; j < read_count; j++) free(files[j].name);
            free(files);
            return 1;
        }
        
        status_t st = parse_line(line, &files[i]);
        free(line);
        if (st == SUCCESS) {
            for (size_t j = 0; j < read_count; j++) {
                if (strcmp(files[j].name, files[i].name) == 0) {
                    st = ERR_INVALID_FORMAT;
                    break;
                }
            }
        }
        if (st != SUCCESS) {
            free(files[i].name);
            fprintf(stderr, "Error: invalid file line %u\n", i + 1);
            for (size_t j = 0; j < read_count; j++) free(files[j].name);
            free(files);
            return 1;
        }
        read_count++;
    }

    OptimalSet plan;
    status_t st = find_optimal_set(files, n, capacity, &plan);
    
    if (st != SUCCESS) {
        fprintf(stderr, "Error: %s\n", status_msg(st));
    } else {
        printf("TOTAL_SIZE %llu\nTOTAL_IMPORTANCE %llu\nFILES %zu\n", 
               plan.total_size, plan.total_importance, plan.count);
        for (size_t i = 0; i < plan.count; i++) {
            printf("%s\n", plan.files[i].name);
        }
        free(plan.files); 
    }

    for (size_t i = 0; i < n; i++) {
        free(files[i].name);
    }
    free(files);

    return st == SUCCESS ? 0 : 1;
}
