#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>

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

typedef struct {
    FILE *file;
    const char *str;
    size_t pos;
    size_t consumed;
    int error;
} Source;

typedef enum { SCAN_MATCH, SCAN_MISMATCH, SCAN_END, SCAN_ERROR } ScanStatus;

static int get_char(Source *src) {
    int c;
    if (src->file) {
        c = fgetc(src->file);
        if (c == EOF && ferror(src->file)) src->error = 1;
    } else c = src->str[src->pos] ? (unsigned char)src->str[src->pos++] : EOF;
    if (c != EOF) src->consumed++;
    return c;
}

static void unget_char(Source *src, int c) {
    if (c == EOF) return;
    if (src->file) {
        if (ungetc(c, src->file) == EOF) { src->error = 1; return; }
    } else if (src->pos > 0) src->pos--;
    if (src->consumed > 0) src->consumed--;
}

static void skip_ws(Source *src) {
    int c;
    while ((c = get_char(src)) != EOF && isspace((unsigned char)c)) { }
    unget_char(src, c);
}

static ScanStatus end_status(const Source *src) {
    return src->error ? SCAN_ERROR : SCAN_END;
}

static ScanStatus read_roman(Source *src, int *out) {
    skip_ws(src);
    int c = get_char(src), previous = 0, found = 0;
    long long total = 0;
    if (c == EOF) return end_status(src);
    while (c != EOF) {
        int value = 0;
        switch (toupper((unsigned char)c)) {
        case 'I': value = 1; break;
        case 'V': value = 5; break;
        case 'X': value = 10; break;
        case 'L': value = 50; break;
        case 'C': value = 100; break;
        case 'D': value = 500; break;
        case 'M': value = 1000; break;
        default: break;
        }
        if (!value) { unget_char(src, c); break; }
        if (previous < value) total -= 2 * previous;
        total += value;
        if (total < 0 || total > INT_MAX) return SCAN_ERROR;
        previous = value;
        found = 1;
        c = get_char(src);
    }
    if (src->error) return SCAN_ERROR;
    if (!found) return SCAN_MISMATCH;
    *out = (int)total;
    return SCAN_MATCH;
}

static ScanStatus read_zeckendorf(Source *src, unsigned int *out) {
    skip_ws(src);
    char bits[sizeof(unsigned int) * CHAR_BIT * 2 + 2];
    size_t length = 0;
    int c = get_char(src);
    if (c == EOF) return end_status(src);
    int terminated = 0;
    while (c == '0' || c == '1') {
        if (length == sizeof(bits)) return SCAN_ERROR;
        bits[length++] = (char)c;
        if (length >= 2 && bits[length - 2] == '1' && bits[length - 1] == '1') {
            terminated = 1;
            break;
        }
        c = get_char(src);
    }
    if (!terminated) unget_char(src, c);
    if (src->error) return SCAN_ERROR;
    if (length < 2 || bits[length - 1] != '1') return SCAN_MISMATCH;
    unsigned long long fib = 1, next = 2, total = 0;
    for (size_t i = 0; i + 1 < length; i++) {
        if (bits[i] == '1') {
            if (fib > UINT_MAX || total > UINT_MAX - fib) return SCAN_ERROR;
            total += fib;
        }
        if (i + 2 < length) {
            if (fib > ULLONG_MAX - next) return SCAN_ERROR;
            unsigned long long sum = fib + next;
            fib = next;
            next = sum;
        }
    }
    *out = (unsigned int)total;
    return SCAN_MATCH;
}

static ScanStatus read_base(Source *src, int base, int upper, int *out) {
    if (base < 2 || base > 36) base = 10;
    skip_ws(src);
    int c = get_char(src);
    if (c == EOF) return end_status(src);
    int negative = (c == '-');
    if (c == '-' || c == '+') c = get_char(src);
    unsigned int limit = (unsigned int)INT_MAX + (unsigned int)negative;
    unsigned int value = 0;
    int found = 0;
    while (c != EOF) {
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (upper && c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else if (!upper && c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        if (digit < 0 || digit >= base) { unget_char(src, c); break; }
        if (value > (limit - (unsigned int)digit) / (unsigned int)base) return SCAN_ERROR;
        value = value * (unsigned int)base + (unsigned int)digit;
        found = 1;
        c = get_char(src);
    }
    if (src->error) return SCAN_ERROR;
    if (!found) return SCAN_MISMATCH;
    if (negative && value == (unsigned int)INT_MAX + 1u) *out = INT_MIN;
    else *out = negative ? -(int)value : (int)value;
    return SCAN_MATCH;
}

static ScanStatus store_count(va_list *args, Length length, size_t count) {
#define STORE(type) do { \
    type *ptr = va_arg(*args, type *); \
    if (!ptr) return SCAN_ERROR; \
    *ptr = (type)count; \
} while (0)
    switch (length) {
    case LEN_HH: STORE(signed char); break;
    case LEN_H: STORE(short); break;
    case LEN_NONE: STORE(int); break;
    case LEN_L: STORE(long); break;
    case LEN_LL: STORE(long long); break;
    case LEN_J: STORE(intmax_t); break;
    case LEN_Z: STORE(signed_size_t); break;
    case LEN_T: STORE(ptrdiff_t); break;
    default: return SCAN_ERROR;
    }
#undef STORE
    return SCAN_MATCH;
}

static ScanStatus scan_standard(Source *src, const char **cursor, va_list *args,
                                 int *out_assigned) {
    const char *start = *cursor;
    const char *p = start + 1;
    int suppressed = (*p == '*');
    if (suppressed) p++;
    if (isdigit((unsigned char)*p)) {
        unsigned int width = 0;
        while (isdigit((unsigned char)*p)) {
            unsigned int digit = (unsigned int)(*p++ - '0');
            if (width > ((unsigned int)INT_MAX - digit) / 10u) return SCAN_ERROR;
            width = width * 10u + digit;
        }
        if (width == 0) return SCAN_ERROR;
    }
    Length length = read_length(&p);
    char conversion = *p;
    if (!conversion || !strchr("diouxXfFeEgGaAcspn[", conversion)) return SCAN_ERROR;
    p++;
    if (conversion == '[') {
        if (*p == '^') p++;
        if (*p == ']') p++;
        while (*p && *p != ']') p++;
        if (*p != ']') return SCAN_ERROR;
        p++;
    }
    if (conversion == 'n') {
        if (suppressed) return SCAN_ERROR;
        ScanStatus st = store_count(args, length, src->consumed);
        if (st == SCAN_MATCH) *cursor = p;
        *out_assigned = 0;
        return st;
    }
    int integer = strchr("diouxX", conversion) != NULL;
    int real = strchr("fFeEgGaA", conversion) != NULL;
    int string = strchr("cs[", conversion) != NULL;
    if ((integer && length == LEN_CAP_L) ||
        (real && length != LEN_NONE && length != LEN_L && length != LEN_CAP_L) ||
        (string && length != LEN_NONE && length != LEN_L) ||
        (conversion == 'p' && length != LEN_NONE)) return SCAN_ERROR;

    size_t size = (size_t)(p - start);
    if (size > SIZE_MAX - 3) return SCAN_ERROR;
    char *spec = malloc(size + 3);
    if (!spec) return SCAN_ERROR;
    memcpy(spec, start, size);
    memcpy(spec + size, "%n", 3);
    int consumed = -1, result = 0;

    // Дополнительный %n сообщает длину фрагмента даже при подавлении присваивания.
#define SCAN_INTO(type) do { \
    type *ptr = va_arg(*args, type *); \
    if (!ptr) { free(spec); return SCAN_ERROR; } \
    result = src->file ? fscanf(src->file, spec, ptr, &consumed) \
                       : sscanf(src->str + src->pos, spec, ptr, &consumed); \
} while (0)
    if (suppressed) {
        result = src->file ? fscanf(src->file, spec, &consumed)
                           : sscanf(src->str + src->pos, spec, &consumed);
    } else if (conversion == 'd' || conversion == 'i') {
        switch (length) {
        case LEN_HH: SCAN_INTO(signed char); break;
        case LEN_H: SCAN_INTO(short); break;
        case LEN_NONE: SCAN_INTO(int); break;
        case LEN_L: SCAN_INTO(long); break;
        case LEN_LL: SCAN_INTO(long long); break;
        case LEN_J: SCAN_INTO(intmax_t); break;
        case LEN_Z: SCAN_INTO(signed_size_t); break;
        case LEN_T: SCAN_INTO(ptrdiff_t); break;
        default: free(spec); return SCAN_ERROR;
        }
    } else if (strchr("ouxX", conversion)) {
        switch (length) {
        case LEN_HH: SCAN_INTO(unsigned char); break;
        case LEN_H: SCAN_INTO(unsigned short); break;
        case LEN_NONE: SCAN_INTO(unsigned int); break;
        case LEN_L: SCAN_INTO(unsigned long); break;
        case LEN_LL: SCAN_INTO(unsigned long long); break;
        case LEN_J: SCAN_INTO(uintmax_t); break;
        case LEN_Z: SCAN_INTO(size_t); break;
        case LEN_T: SCAN_INTO(unsigned_ptrdiff_t); break;
        default: free(spec); return SCAN_ERROR;
        }
    } else if (real) {
        if (length == LEN_CAP_L) { SCAN_INTO(long double); }
        else if (length == LEN_L) { SCAN_INTO(double); }
        else { SCAN_INTO(float); }
    } else if (string) {
        if (length == LEN_L) { SCAN_INTO(wchar_t); }
        else { SCAN_INTO(char); }
    } else { SCAN_INTO(void *); }
#undef SCAN_INTO
    free(spec);
    if (result == EOF) return SCAN_END;
    if (consumed < 0) return SCAN_MISMATCH;
    src->consumed += (size_t)consumed;
    if (!src->file) src->pos += (size_t)consumed;
    *out_assigned = suppressed ? 0 : 1;
    *cursor = p;
    return SCAN_MATCH;
}

static int overvscanf(Source *src, const char *format, va_list incoming) {
    va_list args;
    va_copy(args, incoming);
    int count = 0;
    ScanStatus status = SCAN_MATCH;
    const char *p = format;
    while (*p && status == SCAN_MATCH) {
        if (isspace((unsigned char)*p)) {
            skip_ws(src);
            if (src->error) status = SCAN_ERROR;
            p++;
        } else if (*p == '%' && p[1] != '%') {
            if (strncmp(p, "%Ro", 3) == 0) {
                int *ptr = va_arg(args, int *);
                status = ptr ? read_roman(src, ptr) : SCAN_ERROR;
                if (status == SCAN_MATCH) count++;
                p += 3;
            } else if (strncmp(p, "%Zr", 3) == 0) {
                unsigned int *ptr = va_arg(args, unsigned int *);
                status = ptr ? read_zeckendorf(src, ptr) : SCAN_ERROR;
                if (status == SCAN_MATCH) count++;
                p += 3;
            } else if (strncmp(p, "%Cv", 3) == 0 || strncmp(p, "%CV", 3) == 0) {
                int *ptr = va_arg(args, int *);
                int base = va_arg(args, int);
                status = ptr ? read_base(src, base, p[2] == 'V', ptr) : SCAN_ERROR;
                if (status == SCAN_MATCH) count++;
                p += 3;
            } else {
                int assigned = 0;
                status = scan_standard(src, &p, &args, &assigned);
                if (status == SCAN_MATCH) count += assigned;
            }
        } else {
            if (*p == '%') { skip_ws(src); p++; }
            int c = get_char(src);
            if (c == EOF) status = end_status(src);
            else if (c != (unsigned char)*p) {
                unget_char(src, c);
                status = SCAN_MISMATCH;
            }
            p++;
        }
    }
    va_end(args);
    if (count == 0 && (status == SCAN_END || status == SCAN_ERROR)) return EOF;
    return count;
}

int overfscanf(FILE *stream, const char *format, ...) {
    if (!stream || !format) return EOF;
    Source src = {stream, NULL, 0, 0, 0};
    va_list args;
    va_start(args, format);
    int count = overvscanf(&src, format, args);
    va_end(args);
    return count;
}

int oversscanf(const char *str, const char *format, ...) {
    if (!str || !format) return EOF;
    Source src = {NULL, str, 0, 0, 0};
    va_list args;
    va_start(args, format);
    int count = overvscanf(&src, format, args);
    va_end(args);
    return count;
}
int main(void) {
    int r1, r2, r3;
    unsigned int z;
    
    const char *str = "IV 1a 10011 FF";
    int res = oversscanf(str, "%Ro %Cv %Zr %CV", &r1, &r2, 16, &z, &r3, 16);
    
    if (res == 4) {
        printf("\n");
        printf("Ro (Roman IV): %d\n", r1);     
        printf("Cv (Hex 1a): %d\n", r2);     
        printf("Zr (Zeckendorf 10011): %u\n", z);  
        printf("CV (Hex FF): %d\n", r3);        
    }

    int a; float b; char c;
    if (oversscanf("42 3.14 X", "%d %f %c", &a, &b, &c) == 3) {
        printf("\n\n");
        printf("Int: %d, Float: %.2f, Char: %c\n", a, b, c);
    }

    FILE *f = tmpfile();
    if (f) {
        fputs("MMXXIV 1011", f);
        rewind(f);
        
        int year;
        if (overfscanf(f, "%Ro %Zr", &year, &z) == 2) {
            printf("\n\n");
            printf("Year: %d\n", year);   
            printf("Zr: %u\n", z);        
        }
        fclose(f);
    }

    return 0;
}
