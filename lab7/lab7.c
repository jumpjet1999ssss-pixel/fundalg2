#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

typedef enum {
    OK = 0,
    ERR_ARGS,
    ERR_NULL,
    ERR_MEMORY,
    ERR_INVALID_VECTOR,
    ERR_INVALID_MATRIX,
    ERR_OVERFLOW
} status_t;

static const char *status_msg(status_t s) {
    switch (s) {
    case OK: return "ok";
    case ERR_ARGS: return "invalid arguments";
    case ERR_NULL: return "null pointer";
    case ERR_MEMORY: return "memory allocation failed";
    case ERR_INVALID_VECTOR: return "invalid vector";
    case ERR_INVALID_MATRIX: return "invalid matrix";
    case ERR_OVERFLOW: return "numeric overflow";
    default: return "unknown error";
    }
}

typedef struct {
    int n;
    const double *coords;
} Vector;

typedef struct {
    int n;
    double **data;
} Matrix;

typedef status_t (*NormFunc)(const Vector *, const void *, double, double *);

typedef struct {
    double max_norm;
    int count;
    const Vector **max_vectors;
} NormResult;

static status_t check_vector(const Vector *v) {
    if (!v || !v->coords) return ERR_NULL;
    if (v->n <= 0) return ERR_INVALID_VECTOR;
    for (int i = 0; i < v->n; i++) {
        if (!isfinite(v->coords[i])) return ERR_INVALID_VECTOR;
    }
    return OK;
}

static status_t store_norm(long double value, double *out) {
    if (!out) return ERR_NULL;
    if (!isfinite(value) || value > DBL_MAX) return ERR_OVERFLOW;
    *out = (double)value;
    return OK;
}

static status_t norm_inf(const Vector *v, const void *ctx, double eps, double *out) {
    (void)ctx;
    if (!out) return ERR_NULL;
    if (!isfinite(eps) || eps <= 0.0) return ERR_ARGS;
    status_t st = check_vector(v);
    if (st != OK) return st;
    double maximum = 0.0;
    for (int i = 0; i < v->n; i++) maximum = fmax(maximum, fabs(v->coords[i]));
    *out = maximum;
    return OK;
}

static status_t norm_p(const Vector *v, const void *ctx, double eps, double *out) {
    if (!ctx || !out) return ERR_NULL;
    double p = *(const double *)ctx;
    if (!isfinite(p) || p < 1.0) return ERR_ARGS;
    double scale = 0.0;
    status_t st = norm_inf(v, NULL, eps, &scale);
    if (st != OK) return st;
    if (scale == 0.0) { *out = 0.0; return OK; }
    long double sum = 0.0L;
    for (int i = 0; i < v->n; i++) {
        sum += powl(fabsl((long double)v->coords[i] / scale), p);
    }
    return store_norm((long double)scale * powl(sum, 1.0L / p), out);
}

static status_t norm_A(const Vector *v, const void *ctx, double eps, double *out) {
    if (!ctx || !out) return ERR_NULL;
    double scale = 0.0;
    status_t st = norm_inf(v, NULL, eps, &scale);
    if (st != OK) return st;
    const Matrix *a = ctx;
    if (a->n != v->n || !a->data) return ERR_INVALID_MATRIX;
    size_t n = (size_t)a->n;
    for (size_t i = 0; i < n; i++) {
        if (!a->data[i]) return ERR_INVALID_MATRIX;
        for (size_t j = 0; j < n; j++) {
            if (!isfinite(a->data[i][j])) return ERR_INVALID_MATRIX;
        }
    }
    if (n > SIZE_MAX / n / sizeof(long double)) return ERR_MEMORY;
    long double *lower = calloc(n * n, sizeof(long double));
    if (!lower) return ERR_MEMORY;

    for (size_t i = 0; i < n && st == OK; i++) {
        for (size_t j = 0; j <= i; j++) {
            if (fabsl((long double)a->data[i][j] - a->data[j][i]) > eps) {
                st = ERR_INVALID_MATRIX;
                break;
            }
            long double value = ((long double)a->data[i][j] + a->data[j][i]) / 2.0L;
            for (size_t k = 0; k < j; k++) value -= lower[i * n + k] * lower[j * n + k];
            if (i == j) {
                if (value <= 0.0L) { st = ERR_INVALID_MATRIX; break; }
                lower[i * n + j] = sqrtl(value);
            } else lower[i * n + j] = value / lower[j * n + j];
        }
    }

    if (st == OK) {
        long double length = 0.0L;
        if (scale != 0.0) {
            for (size_t i = 0; i < n; i++) {
                long double component = 0.0L;
                for (size_t j = i; j < n; j++)
                    component += lower[j * n + i] * ((long double)v->coords[j] / scale);
                length = hypotl(length, component);
            }
        }
        st = store_norm(length * scale, out);
    }
    free(lower);
    return st;
}

static status_t find_longest_vectors(NormResult *out_results, double epsilon, int n, int num_vectors, ...) {
    if (!out_results || n <= 0 || num_vectors <= 0 ||
        !isfinite(epsilon) || epsilon <= 0.0) return ERR_ARGS;
    if ((size_t)num_vectors > SIZE_MAX / sizeof(const Vector *) ||
        (size_t)num_vectors > SIZE_MAX / sizeof(double)) return ERR_MEMORY;
    const Vector **vectors = malloc((size_t)num_vectors * sizeof(const Vector *));
    double *values = malloc((size_t)num_vectors * sizeof(double));
    if (!vectors || !values) { free(vectors); free(values); return ERR_MEMORY; }

    va_list ap;
    va_start(ap, num_vectors);
    status_t st = OK;
    for (int i = 0; i < num_vectors; i++) {
        vectors[i] = va_arg(ap, const Vector *);
        st = check_vector(vectors[i]);
        if (st == OK && vectors[i]->n != n) st = ERR_INVALID_VECTOR;
        if (st != OK) break;
    }
    if (st != OK) {
        free(vectors);
        free(values);
        va_end(ap);
        return st;
    }
    int num_norms = va_arg(ap, int);
    if (num_norms <= 0) {
        free(vectors);
        free(values);
        va_end(ap);
        return ERR_ARGS;
    }
    for (int k = 0; k < num_norms; k++) out_results[k] = (NormResult){0};

    for (int k = 0; k < num_norms && st == OK; k++) {
        NormFunc func = va_arg(ap, NormFunc);
        const void *ctx = va_arg(ap, const void *);
        if (!func) { st = ERR_NULL; break; }
        double maximum = 0.0;
        for (int i = 0; i < num_vectors; i++) {
            st = func(vectors[i], ctx, epsilon, &values[i]);
            if (st != OK) break;
            if (!isfinite(values[i]) || values[i] < 0.0) { st = ERR_OVERFLOW; break; }
            maximum = fmax(maximum, values[i]);
        }
        if (st != OK) break;
        int count = 0;
        for (int i = 0; i < num_vectors; i++)
            if (fabs(values[i] - maximum) <= epsilon) count++;
        out_results[k].max_vectors = malloc((size_t)count * sizeof(const Vector *));
        if (!out_results[k].max_vectors) { st = ERR_MEMORY; break; }
        out_results[k].max_norm = maximum;
        out_results[k].count = count;
        int pos = 0;
        for (int i = 0; i < num_vectors; i++)
            if (fabs(values[i] - maximum) <= epsilon)
                out_results[k].max_vectors[pos++] = vectors[i];
    }
    va_end(ap);
    free(values);
    free(vectors);
    if (st != OK) {
        for (int k = 0; k < num_norms; k++) {
            free(out_results[k].max_vectors);
            out_results[k] = (NormResult){0};
        }
    }
    return st;
}

int main(void) {
    int n = 2;
    double c1[] = {3.0, 4.0};
    double c2[] = {-4.0, 3.0};
    double c3[] = {3.0, -4.0};
    double c4[] = {1.0, 1.0};
    
    const Vector v1 = {n, c1};
    const Vector v2 = {n, c2};
    const Vector v3 = {n, c3};
    const Vector v4 = {n, c4};

    double p = 2.0;
    double m_data_arr[] = {2.0, 0.0, 0.0, 2.0};
    double *m_data[] = {m_data_arr, m_data_arr + 2};
    Matrix A = {n, m_data};

    double epsilon = 1e-9;

    {
        printf("\n");
        int num_norms = 3;
        NormResult results[3] = {0}; 
        
        status_t st = find_longest_vectors(results, epsilon, n, 3, &v1, &v2, &v4, num_norms, norm_inf, (const void *)NULL, norm_p, (const void *)&p, norm_A, (const void *)&A);

        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            for (int i = 0; i < num_norms; i++) {
                printf("Norm %d: Max value = %f, Count = %d\n", i + 1, results[i].max_norm, results[i].count);
                for (int j = 0; j < results[i].count; j++) {
                    const Vector *v = results[i].max_vectors[j];
                    printf("  Vector: (%.1f, %.1f)\n", v->coords[0], v->coords[1]);
                }
                free(results[i].max_vectors);
            }
        }
    }
    printf("\n\n");
    
    {
        int num_norms = 1;
        NormResult results[1] = {0};
        
        status_t st = find_longest_vectors(results, epsilon, n, 2, &v1, &v3, num_norms, norm_inf, (const void *)NULL);

        if (st != OK) {
            printf("Error: %s\n", status_msg(st));
        } else {
            printf("Norm Inf: Max value = %f, Count = %d\n", results[0].max_norm, results[0].count);
            for (int j = 0; j < results[0].count; j++) {
                const Vector *v = results[0].max_vectors[j];
                printf("  Vector: (%.1f, %.1f)\n", v->coords[0], v->coords[1]);
            }
            free(results[0].max_vectors);
        }
    }
    printf("\n\n");

    {
        int num_norms = 1;
        NormResult results[1] = {0};
        
        status_t st = find_longest_vectors(results, epsilon, n, -1, num_norms, norm_inf, (const void *)NULL);
        if (st != OK) {
            printf("error: %s\n", status_msg(st));
        }
    }

    return 0;
}
