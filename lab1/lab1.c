#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS = 1,
    ERR_MEMORY_ALLOCATION = 2,
    ERR_INVALID_SEED = 3
} status_t;

static size_t my_strlen(const char *str) {
    if (str == NULL) return 0;
    size_t len = 0;
    while (str[len] != '\0') len++;
    return len;
}

static status_t parse_seed(const char *s, unsigned int *out) {
    if (s == NULL || out == NULL) return ERR_INVALID_ARGUMENTS;
    if (*s == '\0') return ERR_INVALID_SEED;

    unsigned long long val = 0ULL;
    for (const char *p = s; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') return ERR_INVALID_SEED;
        val = val * 10ULL + (unsigned long long)(*p - '0');
        if (val > (unsigned long long)UINT_MAX) return ERR_INVALID_SEED;
    }
    *out = (unsigned int)val;
    return SUCCESS;
}

static const char * status_msg(status_t s) {
    switch (s) {
        case SUCCESS: return "success";
        case ERR_INVALID_ARGUMENTS: return "invalid arguments";
        case ERR_MEMORY_ALLOCATION: return "memory allocation failed";
        case ERR_INVALID_SEED: return "invalid seed";
        default: return "unknown error";
    }
}

// -l

static status_t get_length(const char *str, size_t *out_len) {
    if (str == NULL || out_len == NULL) return ERR_INVALID_ARGUMENTS;
    *out_len = my_strlen(str);
    return SUCCESS;
}

// -r

static status_t get_reversed(const char *str, char **out) {
    if (str == NULL || out == NULL) return ERR_INVALID_ARGUMENTS;
    size_t len = my_strlen(str);
    char *res = (char *)malloc((len + 1) * sizeof(char));
    if (res == NULL) return ERR_MEMORY_ALLOCATION;
    for (size_t i = 0; i < len; i++) res[i] = str[len - 1 - i];
    res[len] = '\0';
    *out = res;
    return SUCCESS;
}

// -u

static status_t get_upper(const char *str, char **out) {
    if (str == NULL || out == NULL) return ERR_INVALID_ARGUMENTS;
    size_t len = my_strlen(str);
    char *res = (char *)malloc((len + 1) * sizeof(char));
    if (res == NULL) return ERR_MEMORY_ALLOCATION;
    for (size_t i = 0; i < len; i++) {
        res[i] = (i % 2 == 1) ? (char)toupper((unsigned char)str[i]) : str[i];
    }
    res[len] = '\0';
    *out = res;
    return SUCCESS;
}

// -n

static status_t group_characters(const char *str, char **out) {
    if (str == NULL || out == NULL) return ERR_INVALID_ARGUMENTS;
    size_t len = my_strlen(str);
    char *res = (char *)malloc((len + 1) * sizeof(char));
    if (res == NULL) return ERR_MEMORY_ALLOCATION;
    size_t pos = 0;
    for (size_t i = 0; i < len; i++)
        if (isdigit((unsigned char)str[i])) res[pos++] = str[i];
    for (size_t i = 0; i < len; i++)
        if (isalpha((unsigned char)str[i])) res[pos++] = str[i];
    for (size_t i = 0; i < len; i++)
        if (!isdigit((unsigned char)str[i]) && !isalpha((unsigned char)str[i]))
            res[pos++] = str[i];
    res[pos] = '\0';
    *out = res;
    return SUCCESS;
}

// -c

static status_t get_random_concat(int count, const char *const *strings, unsigned int seed, char **out) {
    if (strings == NULL || out == NULL || count <= 0) return ERR_INVALID_ARGUMENTS;

    size_t total = 0;
    for (int i = 0; i < count; i++) {
        if (strings[i] == NULL) return ERR_INVALID_ARGUMENTS;
        size_t length = my_strlen(strings[i]);
        if (length > SIZE_MAX - total - 1) return ERR_MEMORY_ALLOCATION;
        total += length;
    }

    char *res = (char *)malloc((total + 1) * sizeof(char));
    if (res == NULL) return ERR_MEMORY_ALLOCATION;

    int *order = (int *)malloc((size_t)count * sizeof(int));
    if (order == NULL) {
        free(res);
        return ERR_MEMORY_ALLOCATION;
    }
    for (int i = 0; i < count; i++) order[i] = i;

    srand(seed);
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    size_t pos = 0;
    for (int i = 0; i < count; i++) {
        const char *s = strings[order[i]];
        size_t slen = my_strlen(s);
        for (size_t k = 0; k < slen; k++) res[pos++] = s[k];
    }
    res[pos] = '\0';

    free(order);
    *out = res;
    return SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n"
        "  %s -l <string>\n"
        "  %s -r <string>\n"
        "  %s -u <string>\n"
        "  %s -n <string>\n"
        "  %s -c <string> <seed> [<string>...]\n", argv[0], argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }

    const char *flag = argv[1];
    const char *str = argv[2];

    if (my_strlen(flag) != 2 || flag[0] != '-') {
        fprintf(stderr, "Invalid flag: %s\n", flag);
        return 1;
    }

    char action = flag[1];
    if (action != 'c' && argc != 3) {
        fprintf(stderr, "Error: this flag requires exactly one string\n");
        return 1;
    }
    status_t status = SUCCESS;

    switch (action) {
        case 'l': {
            size_t len = 0;
            status = get_length(str, &len);
            if (status == SUCCESS) printf("%zu\n", len);
            break;
        }
        case 'r': {
            char *res = NULL;
            status = get_reversed(str, &res);
            if (status == SUCCESS) {
                printf("%s\n", res);
                free(res);
            }
            break;
        }
        case 'u': {
            char *res = NULL;
            status = get_upper(str, &res);
            if (status == SUCCESS) {
                printf("%s\n", res);
                free(res);
            }
            break;
        }
        case 'n': {
            char *res = NULL;
            status = group_characters(str, &res);
            if (status == SUCCESS) {
                printf("%s\n", res);
                free(res);
            }
            break;
        }
        case 'c': {
            if (argc < 4) {
                status = ERR_INVALID_SEED;
                break;
            }

            unsigned int seed = 0;
            status = parse_seed(argv[3], &seed);
            if (status != SUCCESS) break;

            int count = argc - 3;
            char **strings = (char **)malloc((size_t)count * sizeof(char *));
            if (strings == NULL) {
                status = ERR_MEMORY_ALLOCATION;
                break;
            }
            strings[0] = argv[2];
            for (int i = 1; i < count; i++) strings[i] = argv[i + 3];

            char *res = NULL;
            status = get_random_concat(count, (const char *const *)strings, seed, &res);
            free(strings);

            if (status == SUCCESS) {
                printf("%s\n", res);
                free(res);
            }
            break;
        }
        default:
            fprintf(stderr, "Unknown flag: -%c\n", action);
            return 1;
    }

    if (status != SUCCESS) {
        fprintf(stderr, "Error: %s\n", status_msg(status));
        return 1;
    }
    return 0;
}
