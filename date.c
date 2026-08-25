#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#pragma warning(disable:4996)

double Year2JulianDate(int Year) {
	double date = (double)Year-1.0f;
	double d1 = date/100.0f;
	double d2 = 2.0f-d1-d1/4.0;
	double d3 = d1*365.25;

	return d2+d3+1720994.5;
}

static long i_round(double x) {
    return (long)(x + 0.5);
}

static int is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m) {
    static const int dim[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && is_leap(y)) return 29;
    return dim[m];
}

/* 1 月 1 日为第 1 天 */
static int day_of_year(int y, int m, int d) {
    int doy = 0, i;
    for (i = 1; i < m; i++) doy += days_in_month(y, i);
    return doy + d;
}

double ParseEpoch(const char* epoch) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double s = 0.0;
    int n, doy;
    double frac;
    long f8;

    n = sscanf(epoch, "%d-%d-%d %d:%d:%lf", &y, &mo, &d, &h, &mi, &s);
    if (n < 6) n = sscanf(epoch, "%d-%d-%dT%d:%d:%lf", &y, &mo, &d, &h, &mi, &s);
    if (n < 6) return 0;
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
    if (h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0.0 || s >= 61.0) return 0;
    if (d > days_in_month(y, mo)) return 0;

    doy = day_of_year(y, mo, d);
    frac = (h * 3600.0 + mi * 60.0 + s) / 86400.0;
    f8 = i_round(frac * 1e8);
    if (f8 >= 100000000L) {         /* 小数进位到次日 */
        f8 = 0;
        doy++;
    }
    if (doy > (is_leap(y) ? 366 : 365)) {   /* 跨年 */
        doy = 1;
        y++;
    }

    return Year2JulianDate(y) + doy + frac; 
}