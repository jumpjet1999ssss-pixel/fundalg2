#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <wchar.h>

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_NULL,
    ERR_MEMORY,
    ERR_FORMAT
} Status;


typedef enum { LEN_NONE, LEN_HH, LEN_H, LEN_L, LEN_LL, LEN_J, LEN_Z, LEN_T, LEN_CAP_L } Length;

// Знаковый аналог size_t и беззнаковый аналог ptrdiff_t для %z и %t.
#if SIZE_MAX == UINT_MAX
typedef int signed_size_t;
#elif SIZE_MAX == ULONG_MAX
typedef long signed_size_t;
#elif SIZE_MAX == ULLONG_MAX
typedef long long signed_size_t;
#else
#error Unsupported size_t representation
#endif
#if PTRDIFF_MAX == INT_MAX
typedef unsigned int unsigned_ptrdiff_t;
#elif PTRDIFF_MAX == LONG_MAX
typedef unsigned long unsigned_ptrdiff_t;
#elif PTRDIFF_MAX == LLONG_MAX
typedef unsigned long long unsigned_ptrdiff_t;
#else
#error Unsupported ptrdiff_t representation
#endif

static Length read_length(const char **cursor) {
    const char *p = *cursor;
    Length length = LEN_NONE;
    if (*p == 'h') { p++; length = LEN_H; if (*p == 'h') { p++; length = LEN_HH; } }
    else if (*p == 'l') { p++; length = LEN_L; if (*p == 'l') { p++; length = LEN_LL; } }
    else if (*p == 'j') { p++; length = LEN_J; }
    else if (*p == 'z') { p++; length = LEN_Z; }
    else if (*p == 't') { p++; length = LEN_T; }
    else if (*p == 'L') { p++; length = LEN_CAP_L; }
    *cursor = p;
    return length;
}

static Status append_bytes(char **buf, size_t *len, size_t *cap, const char *data, size_t size) {
    if (size > (size_t)INT_MAX - *len) return ERR_MEMORY;
    size_t needed = *len + size + 1;
    if (needed > *cap) {
        size_t next = *cap ? *cap : 64;
        while (next < needed) {
            if (next > SIZE_MAX / 2) { next = needed; break; }
            next *= 2;
        }
        char *tmp = realloc(*buf, next);
        if (!tmp) return ERR_MEMORY;
        *buf = tmp;
        *cap = next;
    }
    memcpy(*buf + *len, data, size);
    *len += size;
    (*buf)[*len] = '\0';
    return OK;
}

static Status append_char(char **buf, size_t *len, size_t *cap, char c) {
    return append_bytes(buf, len, cap, &c, 1);
}

static Status append_str(char **buf, size_t *len, size_t *cap, const char *str) {
    if (!str) return ERR_NULL;
    return append_bytes(buf, len, cap, str, strlen(str));
}
// %Ro
static Status append_roman(int val, char **buf, size_t *len, size_t *cap) {
    if (val <= 0) return ERR_ARGS; 
    const int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char *romans[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    
    for (int i = 0; i < 13; i++) {
        while (val >= values[i]) {
            Status st = append_str(buf, len, cap, romans[i]);
            if (st != OK) return st;
            val -= values[i];
        }
    }
    return OK;
}

// %Zr
static Status append_zeckendorf(unsigned int val, char **buf, size_t *len, size_t *cap) {
    if (val == 0) {
        Status st = append_char(buf, len, cap, '0');
        if (st != OK) return st;
        return append_char(buf, len, cap, '1');
    }
    
    unsigned long long fib[64];
    fib[0] = 1; fib[1] = 2;
    int count = (val == 1) ? 1 : 2;
    while (count >= 2 && fib[count - 1] + fib[count - 2] <= val) {
        fib[count] = fib[count - 1] + fib[count - 2];
        count++;
    }
    
    char bits[64] = {0};
    for (int i = count - 1; i >= 0; i--) {
        if (val >= fib[i]) {
            bits[i] = '1';
            val -= fib[i];
        } else {
            bits[i] = '0';
        }
    }
    
    for (int i = 0; i < count; i++) {
        Status st = append_char(buf, len, cap, bits[i]);
        if (st != OK) return st;
    }
    return append_char(buf, len, cap, '1');
}

// %Cv и %CV
static Status append_base(int val, int base, int is_upper, char **buf, size_t *len, size_t *cap) {
    if (base < 2 || base > 36) base = 10;
    if (val == 0) return append_char(buf, len, cap, '0');
    
    const char *digits = is_upper ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
    char temp[65];
    int pos = 0;
    long long v = val;
    
    if (v < 0) v = -v;
    
    while (v > 0) {
        temp[pos++] = digits[v % base];
        v /= base;
    }
    
    if (val < 0) {
        Status st = append_char(buf, len, cap, '-');
        if (st != OK) return st;
    }
    for (int i = pos - 1; i >= 0; i--) {
        Status st = append_char(buf, len, cap, temp[i]);
        if (st != OK) return st;
    }
    return OK;
}


// %to и %TO: проверяем всю строку, регистр цифр и переполнение.
static Status append_from_base(const char *str, int base, int upper,
                               char **buf, size_t *len, size_t *cap) {
    if (!str) return ERR_NULL;
    if (base < 2 || base > 36) base = 10;
    const char *p = str;
    int negative = (*p == '-');
    if (*p == '-' || *p == '+') p++;
    if (!*p) return ERR_ARGS;
    unsigned long long limit = (unsigned long long)LLONG_MAX + (unsigned int)negative;
    unsigned long long value = 0;
    for (; *p; p++) {
        int digit;
        if (*p >= '0' && *p <= '9') digit = *p - '0';
        else if (upper && *p >= 'A' && *p <= 'Z') digit = *p - 'A' + 10;
        else if (!upper && *p >= 'a' && *p <= 'z') digit = *p - 'a' + 10;
        else return ERR_ARGS;
        if (digit >= base || value > (limit - (unsigned int)digit) / (unsigned int)base)
            return ERR_ARGS;
        value = value * (unsigned int)base + (unsigned int)digit;
    }
    long long result;
    if (negative && value == (unsigned long long)LLONG_MAX + 1ULL) result = LLONG_MIN;
    else result = negative ? -(long long)value : (long long)value;
    char text[sizeof(long long) * CHAR_BIT + 2];
    snprintf(text, sizeof(text), "%lld", result);
    return append_str(buf, len, cap, text);
}

// %mi, %mu, %md, %mf
static Status append_memory_dump(const void *ptr, size_t size, char **buf, size_t *len, size_t *cap) {
    const unsigned char *bytes = (const unsigned char *)ptr;
    for (size_t i = 0; i < size; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            Status st = append_char(buf, len, cap, (bytes[i] & (1 << bit)) ? '1' : '0');
            if (st != OK) return st;
        }
        if (i < size - 1) {
            Status st = append_char(buf, len, cap, ' ');
            if (st != OK) return st;
        }
    }
    return OK;
}


// %n должен учитывать весь уже сформированный вывод, включая нулевые байты.
static Status write_count(va_list *args, Length length, size_t count) {
#define STORE_COUNT(type) do { \
    type *ptr = va_arg(*args, type *); \
    if (!ptr) return ERR_NULL; \
    *ptr = (type)count; \
} while (0)
    switch (length) {
    case LEN_HH: STORE_COUNT(signed char); break;
    case LEN_H: STORE_COUNT(short); break;
    case LEN_NONE: STORE_COUNT(int); break;
    case LEN_L: STORE_COUNT(long); break;
    case LEN_LL: STORE_COUNT(long long); break;
    case LEN_J: STORE_COUNT(intmax_t); break;
    case LEN_Z: STORE_COUNT(signed_size_t); break;
    case LEN_T: STORE_COUNT(ptrdiff_t); break;
    default: return ERR_FORMAT;
    }
#undef STORE_COUNT
    return OK;
}

static Status advance_value(va_list *args, Length length, char conversion) {
    if (strchr("di", conversion)) {
        switch (length) {
        case LEN_HH: case LEN_H: case LEN_NONE: (void)va_arg(*args, int); break;
        case LEN_L: (void)va_arg(*args, long); break;
        case LEN_LL: (void)va_arg(*args, long long); break;
        case LEN_J: (void)va_arg(*args, intmax_t); break;
        case LEN_Z: (void)va_arg(*args, signed_size_t); break;
        case LEN_T: (void)va_arg(*args, ptrdiff_t); break;
        default: return ERR_FORMAT;
        }
    } else if (strchr("ouxX", conversion)) {
        switch (length) {
        case LEN_HH: case LEN_H: case LEN_NONE: (void)va_arg(*args, unsigned int); break;
        case LEN_L: (void)va_arg(*args, unsigned long); break;
        case LEN_LL: (void)va_arg(*args, unsigned long long); break;
        case LEN_J: (void)va_arg(*args, uintmax_t); break;
        case LEN_Z: (void)va_arg(*args, size_t); break;
        case LEN_T: (void)va_arg(*args, unsigned_ptrdiff_t); break;
        default: return ERR_FORMAT;
        }
    } else if (strchr("fFeEgGaA", conversion)) {
        if (length == LEN_CAP_L) (void)va_arg(*args, long double);
        else if (length == LEN_NONE || length == LEN_L) (void)va_arg(*args, double);
        else return ERR_FORMAT;
    } else if (conversion == 'c') {
        if (length == LEN_L) (void)va_arg(*args, wint_t);
        else if (length == LEN_NONE) (void)va_arg(*args, int);
        else return ERR_FORMAT;
    } else if (conversion == 's') {
        if (length == LEN_L) { if (!va_arg(*args, const wchar_t *)) return ERR_NULL; }
        else if (length == LEN_NONE) { if (!va_arg(*args, const char *)) return ERR_NULL; }
        else return ERR_FORMAT;
    } else if (conversion == 'p' && length == LEN_NONE) (void)va_arg(*args, void *);
    else return ERR_FORMAT;
    return OK;
}

static Status append_standard(const char **cursor, va_list *args,
                               char **buf, size_t *len, size_t *cap) {
    const char *start = *cursor;
    const char *p = start + 1;
    while (*p && strchr("-+ #0", *p)) p++;
    int width_star = (*p == '*');
    if (width_star) p++;
    else while (isdigit((unsigned char)*p)) p++;
    int precision_star = 0;
    if (*p == '.') {
        p++;
        precision_star = (*p == '*');
        if (precision_star) p++;
        else while (isdigit((unsigned char)*p)) p++;
    }
    const char *length_start = p;
    Length length = read_length(&p);
    char conversion = *p;
    if (!conversion || !strchr("diouxXfFeEgGaAcspn", conversion)) return ERR_FORMAT;
    if (conversion == 'n') {
        if (length_start != start + 1) return ERR_FORMAT;
        Status st = write_count(args, length, *len);
        if (st == OK) *cursor = p + 1;
        return st;
    }
    size_t size = (size_t)(p - start) + 1;
    char *spec = malloc(size + 1);
    if (!spec) return ERR_MEMORY;
    memcpy(spec, start, size);
    spec[size] = '\0';

    va_list render;
    va_copy(render, *args);
    if (width_star) (void)va_arg(*args, int);
    if (precision_star) (void)va_arg(*args, int);
    Status st = advance_value(args, length, conversion);
    if (st != OK) { va_end(render); free(spec); return st; }
    va_list measure;
    va_copy(measure, render);
    int needed = vsnprintf(NULL, 0, spec, measure);
    va_end(measure);
    if (needed < 0) { va_end(render); free(spec); return ERR_FORMAT; }
    char *text = malloc((size_t)needed + 1);
    if (!text) { va_end(render); free(spec); return ERR_MEMORY; }
    int written = vsnprintf(text, (size_t)needed + 1, spec, render);
    va_end(render);
    free(spec);
    if (written != needed) st = ERR_FORMAT;
    else st = append_bytes(buf, len, cap, text, (size_t)needed);
    free(text);
    if (st == OK) *cursor = p + 1;
    return st;
}
static Status overvsprintf(char **out, size_t *out_len, const char *format, va_list args) {
    if (!out || !out_len || !format) return ERR_NULL;

    char *buf = NULL;
    size_t len = 0, cap = 0;
    Status st = OK;

    va_list copy;
    va_copy(copy, args);
    const char *p = format;

    while (*p) {
        if (*p == '%') {
            if (strncmp(p, "%Ro", 3) == 0) {
                st = append_roman(va_arg(copy, int), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%Zr", 3) == 0) {
                st = append_zeckendorf(va_arg(copy, unsigned int), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%Cv", 3) == 0) {
                int val = va_arg(copy, int);
                int base = va_arg(copy, int);
                st = append_base(val, base, 0, &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%CV", 3) == 0) {
                int val = va_arg(copy, int);
                int base = va_arg(copy, int);
                st = append_base(val, base, 1, &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%to", 3) == 0) {
                const char *str = va_arg(copy, const char *);
                int base = va_arg(copy, int);
                st = append_from_base(str, base, 0, &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%TO", 3) == 0) {
                const char *str = va_arg(copy, const char *);
                int base = va_arg(copy, int);
                st = append_from_base(str, base, 1, &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%mi", 3) == 0) {
                int val = va_arg(copy, int);
                st = append_memory_dump(&val, sizeof(int), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%mu", 3) == 0) {
                unsigned int val = va_arg(copy, unsigned int);
                st = append_memory_dump(&val, sizeof(unsigned int), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%md", 3) == 0) {
                double val = va_arg(copy, double);
                st = append_memory_dump(&val, sizeof(double), &buf, &len, &cap);
                p += 3;
            }
            else if (strncmp(p, "%mf", 3) == 0) {
                double val = va_arg(copy, double);
                float f_val = (float)val;
                st = append_memory_dump(&f_val, sizeof(float), &buf, &len, &cap);
                p += 3;
            }
            else if (*(p + 1) == '%') {
                st = append_char(&buf, &len, &cap, '%');
                p += 2;
            }
            else {
                st = append_standard(&p, &copy, &buf, &len, &cap);
            }
            if (st != OK) break;
        } else {
            st = append_char(&buf, &len, &cap, *p);
            if (st != OK) break;
            p++;
        }
    }

    va_end(copy);

    if (st != OK) {
        free(buf);
        return st;
    }

    if (buf == NULL) {
        buf = (char *)malloc(1);
        if (!buf) return ERR_MEMORY;
        buf[0] = '\0';
    }

    *out = buf;
    *out_len = len;
    return OK;
}

int oversprintf(char *str, const char *format, ...) {
    if (!str) return -1;

    va_list args;
    va_start(args, format);
    char *res = NULL;
    size_t len = 0;
    Status st = overvsprintf(&res, &len, format, args);
    va_end(args);

    if (st != OK || !res) {
        if (str) str[0] = '\0';
        return -1;
    }

    memcpy(str, res, len + 1); 
    
    free(res);
    return (int)len;
}

int overfprintf(FILE *stream, const char *format, ...) {
    if (!stream) return -1;

    va_list args;
    va_start(args, format);
    char *res = NULL;
    size_t len = 0;
    Status st = overvsprintf(&res, &len, format, args);
    va_end(args);

    if (st != OK || !res) return -1;

    size_t written = fwrite(res, 1, len, stream);
    free(res);
    if (written != len) return -1;
    
    return (int)len;
}

int main(void) {
    const size_t BUF_SIZE = 512;
    char *buf = NULL;
    int res = 0;

    printf("\n");
    {
        buf = (char *)malloc(BUF_SIZE);
        if (!buf) return 1;

        res = oversprintf(buf, "Roman: %Ro, Zeckendorf: %Zr", 1984, 100u);
        if (res >= 0) {
            printf("%s\n", buf);
        } else {
            printf("Error: oversprintf failed\n");
        }
        free(buf);
    }
    
    printf("\n");
    {
        buf = (char *)malloc(BUF_SIZE);
        if (!buf) return 1;

        res = oversprintf(buf, "Base 16: %Cv, Base 2: %CV", 255, 16, 255, 2);
        if (res >= 0) {
            printf("%s\n", buf);
        } else {
            printf("Error: oversprintf failed\n");
        }
        free(buf);
    }
    
    printf("\n");
    {
        buf = (char *)malloc(BUF_SIZE);
        if (!buf) return 1;

        res = oversprintf(buf, "From base: %to, Standard int: %d, Float: %0.2f", "ff", 16, 42, 3.14159);
        if (res >= 0) {
            printf("%s\n", buf);
        } else {
            printf("Error: oversprintf failed\n");
        }
        free(buf);
    }
    
    printf("\n");
    {
        res = overfprintf(stdout, "Int 1: %mi\n", 1);
        if (res < 0) printf("Error: overfprintf failed\n");
        
        res = overfprintf(stdout, "Double 1.0: %md\n", 1.0);
        if (res < 0) printf("Error: overfprintf failed\n");
        
        res = overfprintf(stdout, "Float 1.0: %mf\n", 1.0f);
        if (res < 0) printf("Error: overfprintf failed\n");
    }
    
    printf("\n");
    {
        buf = (char *)malloc(BUF_SIZE);
        if (!buf) return 1;

        res = oversprintf(buf, "A%cB%sC", 'x', "");
        if (res >= 0) {
            printf("Result: '%s' (written: %d)\n", buf, res);
        } else {
            printf("Error: oversprintf failed\n");
        }
        free(buf);
    }

    // Дополнительный тест на обработку ошибок (передача NULL в буфер)
    printf("\n");
    {
        res = oversprintf(NULL, "This should fail");
        if (res < 0) {
            printf("Correctly caught error: passing NULL buffer returns %d\n", res);
        } else {
            printf("Error: NULL buffer was not handled\n");
        }
    }

    return 0;
}
