#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#define EPS 1e-12

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS,
    ERR_INVALID_BASE,
    ERR_INVALID_VALUE,
    ERR_PRECISION
} status_t;

static const char *status_msg(status_t s) {
    switch (s) {
    case SUCCESS: return "success";
    case ERR_INVALID_ARGUMENTS: return "invalid arguments";
    case ERR_INVALID_BASE: return "invalid base (must be >= 2)";
    case ERR_INVALID_VALUE: return "invalid value (must be in (0; 1))";
    case ERR_PRECISION: return "cannot approximate fraction at requested precision";
    default: return "unknown error";
    }
}


static unsigned long long gcd(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

static status_t recover_fraction(long double x, double eps,
                                 unsigned long long *out_p, unsigned long long *out_q) {
    if (!out_p || !out_q || !isfinite(x) || x < 1.0L || x >= 10.0L ||
        !isfinite(eps) || eps <= 0.0 || eps >= 1.0) return ERR_INVALID_ARGUMENTS;
    unsigned long long p_prev2 = 0, p_prev = 1;
    unsigned long long q_prev2 = 1, q_prev = 0;
    long double current = x;
    for (int i = 0; i < 128; i++) {
        long double whole = floorl(current);
        if (whole > (long double)ULLONG_MAX) return ERR_PRECISION;
        unsigned long long a = (unsigned long long)whole;
        if ((p_prev && a > (ULLONG_MAX - p_prev2) / p_prev) ||
            (q_prev && a > (ULLONG_MAX - q_prev2) / q_prev)) return ERR_PRECISION;
        unsigned long long p = a * p_prev + p_prev2;
        unsigned long long q = a * q_prev + q_prev2;
        if (q == 0) return ERR_PRECISION;
        if (fabsl((long double)p / q - x) <= eps * x) {
            *out_p = p;
            *out_q = q;
            return SUCCESS;
        }
        long double remainder = current - whole;
        if (remainder == 0.0L) return ERR_PRECISION;
        current = 1.0L / remainder;
        if (!isfinite(current)) return ERR_PRECISION;
        p_prev2 = p_prev; p_prev = p;
        q_prev2 = q_prev; q_prev = q;
    }
    return ERR_PRECISION;
}

static status_t is_finite(double x, int base, double eps, bool *out_is_finite) {
    if (!out_is_finite || !isfinite(eps) || eps <= 0.0 || eps >= 1.0)
        return ERR_INVALID_ARGUMENTS;
    if (base < 2) return ERR_INVALID_BASE;
    if (!isfinite(x) || x <= 0.0 || x >= 1.0) return ERR_INVALID_VALUE;

    long double scaled = x;
    int power = 0;
    while (scaled < 1.0L) {
        scaled *= 10.0L;
        power++;
    }
    unsigned long long numerator, denominator;
    status_t st = recover_fraction(scaled, eps, &numerator, &denominator);
    if (st != SUCCESS) return st;

    int twos = power, fives = power;
    while (twos > 0 && numerator % 2 == 0) { numerator /= 2; twos--; }
    while (fives > 0 && numerator % 5 == 0) { numerator /= 5; fives--; }
    unsigned long long divisor;
    while ((divisor = gcd(denominator, (unsigned int)base)) > 1) denominator /= divisor;
    *out_is_finite = denominator == 1 &&
                    (twos == 0 || base % 2 == 0) &&
                    (fives == 0 || base % 5 == 0);
    return SUCCESS;
}

status_t check_fractions(int base, int count, bool *out_results, ...) {
    if (out_results == NULL) return ERR_INVALID_ARGUMENTS;
    if (base < 2) return ERR_INVALID_BASE;
    if (count <= 0) return ERR_INVALID_ARGUMENTS;

    va_list ap;
    va_start(ap, out_results);

    for (int i = 0; i < count; i++) {
        double x = va_arg(ap, double);
        
        status_t st = is_finite(x, base, EPS, &out_results[i]);
        if (st != SUCCESS) {
            va_end(ap);
            return st;
        }
    }

    va_end(ap);
    return SUCCESS;
}

int main(void) {
    {
        int count = 3;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        double a = 0.5;
        double b = 0.25;
        double c = 1.0 / 3.0;

        status_t st = check_fractions(10, count, results, a, b, c);
        if (st == SUCCESS) {
            printf("0.5  -> %s\n", results[0] ? "finite" : "infinite");
            printf("0.25 -> %s\n", results[1] ? "finite" : "infinite");
            printf("1/3  -> %s\n", results[2] ? "finite" : "infinite");
        } else {
            printf("Error: %s\n", status_msg(st));
        }
        free(results);
    }
    printf("\n");
    {
        int count = 2;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        double a = 0.5;
        double b = 0.1;

        status_t st = check_fractions(2, count, results, a, b);
        if (st == SUCCESS) {
            printf("0.5 -> %s\n", results[0] ? "finite" : "infinite");
            printf("0.1 -> %s\n", results[1] ? "finite" : "infinite");
        } else {
            printf("Error: %s\n", status_msg(st));
        }
        free(results);
    }
    printf("\n");
    {
        int count = 2;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        double a = 0.5;
        double b = 1.0 / 3.0;

        status_t st = check_fractions(3, count, results, a, b);
        if (st == SUCCESS) {
            printf("0.5  -> %s\n", results[0] ? "finite" : "infinite");
            printf("1/3  -> %s\n", results[1] ? "finite" : "infinite");
        } else {
            printf("Error: %s\n", status_msg(st));
        }
        free(results);
    }
    printf("\n");
    {
        int count = 1;
        bool *results = (bool *)malloc(count * sizeof(bool));
        if (!results) return 1;

        status_t st = check_fractions(10, count, results, 1.5);
        if (st == SUCCESS) {
            printf("1.5 -> %s\n", results[0] ? "finite" : "infinite");
        } else {
            printf("Expected error: %s\n", status_msg(st));
        }
        free(results);
    }

    return 0;
}
