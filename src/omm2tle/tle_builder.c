/*  TLE Builder -- Turn parsed data (in struct omm_t) into TLE  */

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "omm2tle.h"
#include "tool/string_operation.h"


/* ------------------------- TLE 格式化 ------------------------- */

/*
 * OMM 的 OBJECT_ID "1998-067A" -> TLE 国际编号 "98067A  " (8 字符, 左对齐)。
 * 解析失败输出 8 个空格。
 */
static void format_object_id(const char* id, char out[9]) {
    int year = 0, num = 0;
    char piece = ' ';
    int n;
    if (!id || !*id) { memcpy(out, "        ", 8); out[8] = '\0'; return; }
    /* "1998-067A" : 年份4位 + '-' + 发射序号3位 + 部件字母(无连字符) */
    n = sscanf(id, "%4d-%3d%c", &year, &num, &piece);
    if (n < 2 || year < 1957 || year > 2100) {
        memcpy(out, "        ", 8);
        out[8] = '\0';
        return;
    }
    if (n == 2) piece = ' ';
    if (piece >= 'a' && piece <= 'z') piece = (char)(piece - 'a' + 'A');
    snprintf(out, 9, "%02d%03d%c", year % 100, num, piece);
}

/*
 * OMM ISO 8601 历元 "YYYY-MM-DD[ T]HH:MM:SS[.ffffff]" -> TLE "YYDDD.DDDDDDDD"。
 * 成功返回 1; 失败返回 0。
 */
static int format_epoch(const char* iso, char out[15]) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double s = 0.0;
    int n, doy;
    double frac;
    long f8;

    n = sscanf(iso, "%d-%d-%d %d:%d:%lf", &y, &mo, &d, &h, &mi, &s);
    if (n < 6) n = sscanf(iso, "%d-%d-%dT%d:%d:%lf", &y, &mo, &d, &h, &mi, &s);
    if (n < 6) return 0;
    if (y < 1957 || y > 2100) return 0;
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
    snprintf(out, 15, "%02d%03d.%08ld", y % 100, doy, f8);
    return 1;
}

/*
 * TLE 第 1 行 34-43 列: 一阶导数, 10 字符 "[±].NNNNNNNN"。
 * 例: 0.00009536 -> " .00009536"; -0.000083585 -> "-.00008359"。
 */
static void format_ndot(double v, char out[11]) {
    char sign = (v < 0.0) ? '-' : ' ';
    long digits = i_round(fabs(v) * 1e8);
    if (digits > 99999999L) digits = 99999999L;
    snprintf(out, 11, "%c.%08ld", sign, digits);
}

/*
 * TLE 第 1 行 45-52 / 54-61 列: 隐含小数点 + 指数, 8 字符 "[±]MMMMM[±]E"。
 * 例: 0.00017760953 -> " 17761-3"; 0 -> " 00000+0"; -0.00010270 -> "-10270-3"。
 */
static void format_exp(double v, char out[9]) {
    char sign;
    double av;
    int exp = 0;
    long digits;

    if (v == 0.0 || fabs(v) < 5e-10) { strcpy(out, " 00000+0"); return; }
    sign = (v < 0.0) ? '-' : ' ';
    av = fabs(v);
    while (av >= 1.0) { av /= 10.0; exp++; }
    while (av < 0.1) { av *= 10.0; exp--; }
    if (exp < -9) { strcpy(out, " 00000+0"); return; }      /* 过小按 0 */
    if (exp > 9) { snprintf(out, 9, "%c99999+9", sign); return; }
    digits = i_round(av * 1e5);
    if (digits >= 100000L) { digits = 10000L; exp++; }      /* 尾数进位 */
    if (exp > 9) { snprintf(out, 9, "%c99999+9", sign); return; }
    if (exp >= 0) snprintf(out, 9, "%c%05ld+%d", sign, digits, exp);
    else          snprintf(out, 9, "%c%05ld-%d", sign, digits, -exp);
}

/* TLE 校验和: 对前 68 列, 数字按值, 减号计 1, 其它计 0, 模 10 */
static int tle_checksum(const char* s) {
    int sum = 0, i;
    for (i = 0; i < 68 && s[i] != '\0'; i++) {
        char ch = s[i];
        if (ch >= '0' && ch <= '9') sum += ch - '0';
        else if (ch == '-') sum += 1;
        /* '+' 及其它字符计 0 */
    }
    return sum % 10;
}

/*
 * 由 OMM 参数生成三行 TLE(核心转换)。
 * raw_ndot/raw_nddot: 1 表示将 OMM 值原样写入; 0 表示除以 2 / 除以 6。
 */
void build_lines(const omm_t* o, int raw_ndot, int raw_nddot,
    char* line0, char* line1, char* line2) {
    double ndot = raw_ndot ? o->ndot : o->ndot / 2.0;
    double nddot = raw_nddot ? o->nddot : o->nddot / 6.0;
    double incl = fmod(o->inclination, 360.0);
    double raan = fmod(o->raan, 360.0);
    double argp = fmod(o->argp, 360.0);
    double ma = fmod(o->mean_anomaly, 360.0);
    double mm = o->mean_motion;
    long norad = o->norad_id;
    long elem = o->element_set;
    long rev = o->rev;
    long ecci;
    char intl[9], epoch[15], ndotf[11], nddotf[9], bstarf[9];
    char name[OMM_NAME_BUF];

    /* 角度归一化到 [0, 360) */
    if (incl < 0) incl += 360.0;
    if (raan < 0) raan += 360.0;
    if (argp < 0) argp += 360.0;
    if (ma < 0) ma += 360.0;

    /* 范围检查与截断 */
    if (norad < 0) norad = 0;
    if (norad > 99999) { norad = 99999; }
    if (elem < 0) elem = 0;
    if (elem > 9999) { fprintf(stderr, "Warn: Enement number %ld exceed 4 digits, trunc\n", elem); elem = 9999; }
    if (rev < 0) { fprintf(stderr, "Warn:  Negative Orbit # %ld for NORAD # %ld , Replace with 0\n", rev, norad); rev = 0; }
    if (rev > 99999) { fprintf(stderr, "Warn: Orbit # %ld exceed 5 digits, trunc\n", rev); rev = 99999; }
    if (mm < 0.0) { fprintf(stderr, "Warn: Negative mean motion, replace with 0\n"); mm = 0.0; }
    if (mm >= 100.0) { fprintf(stderr, "Warn: Mean motion %.6f exceed range TLE can repersent, trunc\n", mm); mm = 99.99999999; }
    ecci = i_round(o->eccentricity * 1e7);
    if (o->eccentricity < 0.0 || o->eccentricity >= 1.0) {
        fprintf(stderr, "Warn: Eccentricy % .8f exceed range[0, 1), trunc\n", o->eccentricity);
        if (o->eccentricity < 0.0) ecci = 0; else ecci = 9999999;
    }
    if (ecci < 0) ecci = 0;
    if (ecci > 9999999) ecci = 9999999;

    /* 第 0 行: 卫星名称*/
    if (o->object_name[0]) {
        snprintf(name, OMM_NAME_BUF, "%s", o->object_name);
    }
    else {
        snprintf(name, OMM_NAME_BUF, "%05ld", norad);   /* 无名称时用目录号 */
    }
    snprintf(line0, OMM_NAME_BUF, "%-0.32s", name);

    /* 第 1 行 */
    format_object_id(o->object_id, intl);
    if (!format_epoch(o->epoch, epoch)) {
        /* 解析阶段已校验, 此处不应发生 */
        snprintf(epoch, sizeof(epoch), "00000.00000000");
    }
    format_ndot(ndot, ndotf);
    format_exp(nddot, nddotf);
    format_exp(o->bstar, bstarf);
    snprintf(line1, OMM_TLE_BUF,
        "1 %05ld%c %-8s %14s %10s %8s %8s %c %4ld",
        norad, o->classification, intl, epoch, ndotf, nddotf, bstarf,
        o->ephemeris_type, elem);
    line1[68] = (char)('0' + tle_checksum(line1));
    line1[69] = '\0';

    /* 第 2 行 */
    snprintf(line2, OMM_TLE_BUF,
        "2 %05ld %8.4f %8.4f %07ld %8.4f %8.4f %11.8f%5ld",
        norad, incl, raan, ecci, argp, ma, mm, rev);
    line2[68] = (char)('0' + tle_checksum(line2));
    line2[69] = '\0';
}