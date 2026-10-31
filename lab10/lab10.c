#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>

typedef enum {
    SUCCESS = 0,
    ERR_INVALID_ARGUMENTS,
    ERR_MEMORY_ALLOCATION,
    ERR_OVERFLOW
} status_t;

static const char *status_msg(status_t s) {
    switch (s) {
    case SUCCESS: return "success";
    case ERR_INVALID_ARGUMENTS: return "invalid arguments";
    case ERR_MEMORY_ALLOCATION: return "memory allocation failed";
    case ERR_OVERFLOW: return "numeric overflow";
    default: return "unknown error";
    }
}

status_t shift_polynomial(double eps, double a, double **out_g, int n, ...) {
    if (!out_g) return ERR_INVALID_ARGUMENTS;
    *out_g = NULL;
    if (n < 0 || n == INT_MAX || !isfinite(eps) || eps < 0.0 || !isfinite(a))
        return ERR_INVALID_ARGUMENTS;
    size_t count = (size_t)n + 1;
    if (count > SIZE_MAX / sizeof(double)) return ERR_MEMORY_ALLOCATION;

    double *f = (double *)malloc(count * sizeof(double));
    if (f == NULL) return ERR_MEMORY_ALLOCATION;

    double *g = (double *)malloc(count * sizeof(double));
    if (g == NULL) {
        free(f);
        return ERR_MEMORY_ALLOCATION;
    }

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i <= n; i++) {
        f[i] = va_arg(ap, double);
        if (!isfinite(f[i])) {
            va_end(ap);
            free(f);
            free(g);
            return ERR_INVALID_ARGUMENTS;
        }
    }
    va_end(ap);

    // Повторная схема Горнера даёт коэффициенты при степенях (x - a).
    for (int k = 0; k <= n; k++) {
        for (int i = n - 1; i >= k; i--) {
            f[i] = fma(a, f[i + 1], f[i]);
            if (!isfinite(f[i])) {
                free(f);
                free(g);
                return ERR_OVERFLOW;
            }
        }
        g[k] = f[k];
        
        if (fabs(g[k]) < eps) {
            g[k] = 0.0;
        }
    }

    free(f);
    *out_g = g;
    return SUCCESS;
}

static void print_polynom(const char *label, const double *coeffs, int n, double a) {
    printf("%s(x) = ", label);
    for (int i = 0; i <= n; i++) {
        double val = coeffs[i];
        
        if (i > 0) {
            if (val >= 0.0) printf(" + ");
            else printf(" - ");
            val = fabs(val);
        } else if (val < 0.0) {
            printf("-");
            val = fabs(val);
        }

        if (i == 0) {
            printf("%.*g", DBL_DIG + 2, val);
        } else if (i == 1) {
            printf("%.*g*(x - %.*g)", DBL_DIG + 2, val, DBL_DIG + 2, a);
        } else {
            printf("%.*g*(x - %.*g)^%d", DBL_DIG + 2, val, DBL_DIG + 2, a, i);
        }
    }
    printf("\n");
} 

int main(void) {
    double *g = NULL;

    {
        status_t st = shift_polynomial(1e-9, 1.0, &g, 2, 1.0, 2.0, 3.0);
        printf("f(x) = 1 + 2x + 3x^2, a = 1.0\n");
        if (st == SUCCESS) {
            print_polynom("g", g, 2, 1.0);
            free(g);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    {
        status_t st = shift_polynomial(1e-9, 2.0, &g, 3, 0.0, 0.0, 0.0, 1.0);
        printf("\nf(x) = x^3, a = 2.0\n");
        if (st == SUCCESS) {
            print_polynom("g", g, 3, 2.0);
            free(g);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    {
        status_t st = shift_polynomial(1e-9, 3.0, &g, 0, 5.0);
        printf("\nf(x) = 5, a = 3.0\n");
        if (st == SUCCESS) {
            print_polynom("g", g, 0, 3.0);
            free(g);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    {
        status_t st = shift_polynomial(1e-9, 2.0, &g, 2, 10.0, -4.0, 1.0);
        printf("\nf(x) = 10 - 4x + x^2, a = 2.0\n");
        if (st == SUCCESS) {
            print_polynom("g", g, 2, 2.0);
            free(g);
        } else {
            printf("Error: %s\n", status_msg(st));
        }
    }

    return 0;
}
