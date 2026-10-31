#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS,
    ERR_INVALID_BASE,
    ERR_INVALID_FORMAT,
    ERR_MEMORY_ALLOCATION
} status_t;

static const char *status_msg(status_t s) {
    switch (s) {
    case SUCCESS: return "success";
    case ERR_INVALID_ARGUMENTS: return "invalid arguments";
    case ERR_INVALID_BASE: return "invalid base (must be 2..36)";
    case ERR_INVALID_FORMAT: return "invalid number format";
    case ERR_MEMORY_ALLOCATION: return "memory allocation failed";
    default: return "unknown error";
    }
}

// вспомогательные функции

static int char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}

static char val_to_char(int v) {
    if (v >= 0 && v <= 9) return (char)('0' + v);
    return (char)('A' + (v - 10));
}

static status_t validate_number(const char *s, int base) {
    if (s == NULL || *s == '\0') return ERR_INVALID_FORMAT;
    
    size_t len = strlen(s);

    for (size_t i = 0; i < len; i++) {
        int val = char_to_val(s[i]);
        if (val < 0 || val >= base) return ERR_INVALID_FORMAT;
    }
    return SUCCESS;
}

static status_t add_numbers_in_base(const char *a, const char *b, int base, char **out_res) {
    if (a == NULL || b == NULL || out_res == NULL) return ERR_INVALID_ARGUMENTS;
    if (base < 2 || base > 36) return ERR_INVALID_BASE;

    status_t st = validate_number(a, base);
    if (st != SUCCESS) return st;
    st = validate_number(b, base);
    if (st != SUCCESS) return st;

    // Ведущие нули допустимы во входных данных, но не в результате.
    while (a[0] == '0' && a[1] != '\0') a++;
    while (b[0] == '0' && b[1] != '\0') b++;

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    size_t max_len = (len_a > len_b) ? len_a : len_b;

    char *res = (char *)malloc((max_len + 2)*sizeof(char));
    if (res == NULL) return ERR_MEMORY_ALLOCATION;

    int carry = 0;
    size_t pos = 0;
    size_t i = len_a;
    size_t j = len_b;

    while (i > 0 || j > 0 || carry > 0) {
        int sum = carry;
        if (i > 0) sum += char_to_val(a[--i]);
        if (j > 0) sum += char_to_val(b[--j]);
        
        res[pos++] = val_to_char(sum % base);
        carry = sum / base;
    }
    res[pos] = '\0';

    for (size_t k = 0; k < pos / 2; k++) {
        char tmp = res[k];
        res[k] = res[pos - 1 - k];
        res[pos - 1 - k] = tmp;
    }

    *out_res = res;
    return SUCCESS;
}

status_t sum_all_in_base(int base, int count, char **out_res, ...) {
    if (out_res == NULL) return ERR_INVALID_ARGUMENTS;
    if (base < 2 || base > 36) return ERR_INVALID_BASE;
    if (count < 0) return ERR_INVALID_ARGUMENTS;

    char *current_sum = (char *)malloc(2*sizeof(char));
    if (!current_sum) return ERR_MEMORY_ALLOCATION;
    current_sum[0] = '0'; current_sum[1] = '\0';

    va_list ap;
    va_start(ap, out_res);

    for (int i = 0; i < count; i++) {
        const char *num_str = va_arg(ap, const char *);
        
        char *next_sum = NULL;
        status_t st = add_numbers_in_base(current_sum, num_str, base, &next_sum);
        if (st != SUCCESS) {
            free(current_sum);
            va_end(ap);
            return st;
        }

        free(current_sum);
        current_sum = next_sum;
    }

    va_end(ap);

    *out_res = current_sum;
    return SUCCESS;
}

int main(void) {
    {
        char *result = NULL;
        status_t st = sum_all_in_base(10, 2, &result, "123", "456");
        if (st == SUCCESS) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }
    printf("\n");
    {
        char *result = NULL;
        status_t st = sum_all_in_base(16, 2, &result, "FFFFFFFFFFFFFFFF", "1");
        if (st == SUCCESS) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }
    printf("\n");
    {
        char *result = NULL;
        status_t st = sum_all_in_base(2, 3, &result, "111", "1", "10");
        if (st == SUCCESS) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }
    printf("\n");
    {
        char *result = NULL;
        status_t st = sum_all_in_base(10, 2, &result, "0", "0");
        if (st == SUCCESS) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }
    printf("\n");
    {
        char *result = NULL;
        status_t st = sum_all_in_base(10, 2, &result, "12A", "45");
        if (st == SUCCESS) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Expected error: %s\n", status_msg(st));
        }
    }
    printf("\n");
    {
        char *result = NULL;
        status_t st = sum_all_in_base(10, 2, &result, "012", "45");
        if (st == SUCCESS) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Expected error: %s\n", status_msg(st));
        }
    }

    return 0;
}
