#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------- 小工具 ------------------------- */

/* 四舍五入(仅用于非负值) */
long i_round(double x) {
    return (long)(x + 0.5);
}

int is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int days_in_month(int y, int m) {
    static const int dim[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && is_leap(y)) return 29;
    return dim[m];
}

/* 1 月 1 日为第 1 天 */
int day_of_year(int y, int m, int d) {
    int doy = 0, i;
    for (i = 1; i < m; i++) doy += days_in_month(y, i);
    return doy + d;
}

/* 大小写不敏感字符串比较 */
 int name_eq(const char* a, const char* b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* 去除首尾空白 */
void trim(char* s) {
    char* p = s;
    size_t n;
    while (*p == ' ' || *p == '\t') p++;
    n = strlen(p);
    while (n > 1 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r' || p[n - 1] == '\n')) {
        p[--n] = '\0';
    }
    if (p != s) memmove(s, p, n + 1);
}

/* 严格解析整数(允许首尾空白) */
int parse_int(const char* s, long* out) {
    char* end = NULL;
    long v;
    if (!s) return 0;
    v = strtol(s, &end, 10);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

/* 严格解析整数(允许首尾空白) */
int parse_uint(const char* s, long* out) {
    char* end = NULL;
    long v;
    if (!s) return 0;
    v = strtoul(s, &end, 10);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

/* 严格解析浮点数(允许首尾空白, 支持 1.23E-4 形式) */
int parse_dbl(const char* s, double* out) {
    char* end = NULL;
    double v;
    if (!s) return 0;
    v = strtod(s, &end);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}