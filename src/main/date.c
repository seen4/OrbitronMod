#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#pragma warning(disable:4996)

double Year2JulianDate(int Year) {
    //REF:https://blog.csdn.net/weixin_42763614/article/details/82880007
    Year = Year - 1;
    double B= 2 - floor(Year / 100) + floor(Year / 400);
	double date = floor(365.25*(Year+4716)) + floor(30.6*(14)) + B - 1524.5;

	return date;
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
    frac = ((double)h * 3600.0 + (double)mi * 60.0 + s) / 86400.0;
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

void JulianDate2Gregorian(double jd, uint16_t *year, uint16_t *month, uint16_t *day, uint16_t *hour, uint16_t *minute, uint16_t *second) {
    //REF:https://blog.csdn.net/qq_24172609/article/details/112244135
    double frac=modf(jd,&jd);
    if (jd > 2299161) {
        jd=jd+10;
        double a = (int)((jd - 2268993) / 36524.25);
        jd=jd+a-(int)((a+3)/4);
    }

    uint16_t y=(int)(jd/365.25)-4712;
    *year=y;

    uint16_t muYD=0,m=0;
    while (1) {
        muYD = jd-(int)((y+4712)*365.25)-1;
        if (muYD >= 59) {
            m=(int)((muYD+1+63)/30.61)-1;
            *month=m>12 ? m-12 : m;
            break;
        }
        else {
            y=y-1;
        }
    }

    *day = muYD-(int)(30.61*(m+1))+63+1;

    double hh=0,mm=0,ss=0;
    frac=modf(frac*24,&hh);
    frac=modf(frac*60,&mm);
    ss=frac*60;

    *hour=(int)hh,*minute=(int)mm;*second=(int)ss;
    return;
}
