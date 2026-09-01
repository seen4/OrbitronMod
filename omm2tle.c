/*
 * omm2tle.c — 将 OMM(Orbit Mean-Element Message) CSV 格式星历转换为 TLE 三行格式
 *
 * 输入格式参考: CelesTrak gp.php ... FORMAT=csv
 *   (https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=csv)
 * 输出格式参考: projectpluto tle_info
 *   (https://www.projectpluto.com/tle_info.htm)
 *
 * 输出约定:
 *   - 第 0 行: 卫星名称, 固定 24 字符(不足补空格, 超出截断)
 *   - 第 1 行: 行号/卫星号/分类/国际编号/历元/ndot/nddot/BSTAR/星历类型/元素集号/校验和
 *   - 第 2 行: 行号/卫星号/倾角/升交点赤经/偏心率/近地点幅角/平近点角/平运动/圈数/校验和
 *   - 校验和: 数字按值累加, 减号计 1, 加号/空格/点/字母计 0, 总和模 10
 *
 * 核心转换逻辑封装为 omm_next_tle()(见 omm2tle.h):
 *   int omm_next_tle(FILE *fp, char *name, char *line1, char *line2);
 * 每次调用从文件流读取一条 OMM 记录, 转换后写入三个输出缓冲区。
 *
 * 关于 MEAN_MOTION_DOT / MEAN_MOTION_DDOT:
 *   CelesTrak 系 OMM CSV 中的 MEAN_MOTION_DOT 即 TLE 第 1 行 34-43 列的字段值
 *   (已经是一阶导数的二分之一), MEAN_MOTION_DDOT 同理, 因此默认原样写入。
 *   若数据源按 CCSDS 严格定义给出"真实"一阶/二阶导数, 可调用
 *   omm_set_options(1, 1, ...) 分别除以 2 和 6 后再写入。
 */

/* MSVC 对 strcpy/sscanf/fopen 等的 C4996 安全警告: 本程序所有使用处均已限定
   缓冲区大小, 属于安全用法, 故在 MSVC 下抑制该提示(gcc/clang 不受影响)。
   注意: 必须在包含 CRT 头文件之前定义。 */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "omm2tle.h"

#define VERSION     "1.1.0"
#define FIELD_BUF   1024        /* 单个 CSV 字段最大长度 */
#define LINE_BUF    16384       /* CSV 记录行缓冲(含表头) */


/* ------------------------- 数据结构 ------------------------- */

/* 列索引映射(按表头名称查找) */
typedef struct {
    int object_name, object_id, epoch;
    int mean_motion, eccentricity, inclination, raan, argp, mean_anomaly;
    int ephemeris_type, classification, norad_id, element_set, rev;
    int bstar, ndot, nddot, decayed;
    int tle0, tle1, tle2;       /* 可选: CSV 自带的 TLE 参考行(用于 --check) */
} cols_t;


/* CSV 解析游标(指向一行内存缓冲) */
typedef struct {
    const char *cur;
    const char *end;
} csvr_t;

/* ------------------------- 小工具 ------------------------- */

/* 四舍五入(仅用于非负值) */
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

/* 大小写不敏感字符串比较 */
static int name_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* 去除首尾空白 */
static void trim(char *s) {
    char *p = s;
    size_t n;
    while (*p == ' ' || *p == '\t') p++;
    n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r' || p[n - 1] == '\n')) {
        p[--n] = '\0';
    }
    if (p != s) memmove(s, p, n + 1);
}

/* 严格解析整数(允许首尾空白) */
static int parse_int(const char *s, long *out) {
    char *end = NULL;
    long v;
    if (!s) return 0;
    v = strtol(s, &end, 10);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

/* 严格解析浮点数(允许首尾空白, 支持 1.23E-4 形式) */
static int parse_dbl(const char *s, double *out) {
    char *end = NULL;
    double v;
    if (!s) return 0;
    v = strtod(s, &end);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

/* ------------------------- CSV 解析 ------------------------- */

static void free_csv_fields(char **f, int n) {
    int i;
    if (!f) return;
    for (i = 0; i < n; i++) free(f[i]);
    free(f);
}

/*
 * 解析一条 CSV 记录(一行)的字段。
 * 支持: 双引号包裹的字段、引号内的逗号、"" 转义、CRLF。
 * 返回 1 表示成功(字段在 *fields / *nfields, 需 free_csv_fields),
 * 返回 0 表示游标处无记录(空行/行尾)。
 */
static int csv_next_record(csvr_t *r, char ***fields, int *nfields) {
    char **arr = NULL;
    int n = 0, cap = 0;

    *fields = NULL;
    *nfields = 0;

    /* 跳过行首的换行符(空行) */
    while (r->cur < r->end && (*r->cur == '\n' || *r->cur == '\r')) r->cur++;
    if (r->cur >= r->end) return 0;

    for (;;) {
        char buf[FIELD_BUF];
        size_t len = 0;

        /* --- 解析一个字段 --- */
        if (r->cur < r->end && *r->cur == '"') {
            r->cur++;                       /* 开引号 */
            for (;;) {
                char c;
                if (r->cur >= r->end) break; /* 未闭合引号: 容忍 */
                c = *r->cur;
                if (c == '"') {
                    if (r->cur + 1 < r->end && r->cur[1] == '"') { /* "" -> 字面引号 */
                        if (len + 1 < FIELD_BUF) buf[len++] = '"';
                        r->cur += 2;
                        continue;
                    }
                    r->cur++;               /* 闭引号 */
                    break;
                }
                if (len + 1 < FIELD_BUF) buf[len++] = c;
                r->cur++;
            }
            /* 闭引号后到逗号/换行前的多余内容忽略 */
            while (r->cur < r->end && *r->cur != ',' && *r->cur != '\n') {
                if (*r->cur == '\r') { r->cur++; continue; }
                r->cur++;
            }
        } else {
            while (r->cur < r->end && *r->cur != ',' && *r->cur != '\n' && *r->cur != '\r') {
                if (len + 1 < FIELD_BUF) buf[len++] = *r->cur;
                r->cur++;
            }
        }
        buf[len] = '\0';
        trim(buf);

        /* --- 加入字段数组 --- */
        if (n == cap) {
            char **na;
            cap = cap ? cap * 2 : 8;
            na = (char **)realloc(arr, (size_t)cap * sizeof(char *));
            if (!na) {
                fprintf(stderr, "错误: 内存不足\n");
                free_csv_fields(arr, n);
                exit(1);
            }
            arr = na;
        }
        arr[n] = (char *)malloc(strlen(buf) + 1);
        if (!arr[n]) {
            fprintf(stderr, "错误: 内存不足\n");
            free_csv_fields(arr, n);
            exit(1);
        }
        strcpy(arr[n], buf);
        n++;

        /* --- 检查分隔符 --- */
        if (r->cur < r->end && *r->cur == ',') { r->cur++; continue; }
        if (r->cur < r->end && *r->cur == '\n') r->cur++;
        else if (r->cur < r->end && *r->cur == '\r') {
            r->cur++;
            if (r->cur < r->end && *r->cur == '\n') r->cur++;
        }
        break;                              /* 记录结束 */
    }

    *fields = arr;
    *nfields = n;
    return 1;
}

/* 在表头中查找列名, 返回列号; 未找到返回 -1 */
static int find_col(char **hdr, int nhdr, const char *name) {
    int i;
    for (i = 0; i < nhdr; i++) {
        if (name_eq(hdr[i], name)) return i;
    }
    return -1;
}

static void init_cols(cols_t *c) {
    c->object_name = c->object_id = c->epoch = -1;
    c->mean_motion = c->eccentricity = c->inclination = c->raan = c->argp = -1;
    c->mean_anomaly = c->ephemeris_type = c->classification = c->norad_id = -1;
    c->element_set = c->rev = c->bstar = c->ndot = c->nddot = c->decayed = -1;
    c->tle0 = c->tle1 = c->tle2 = -1;
}

/* 取第 idx 列的内容; 列不存在或为空返回 NULL */
static const char *field_of(char **f, int nf, int idx) {
    if (idx < 0 || idx >= nf) return NULL;
    if (!f[idx] || !f[idx][0]) return NULL;
    return f[idx];
}

/* ------------------------- 解析一行 OMM ------------------------- */

static int parse_row(char **f, int nf, const cols_t *c, omm_t *o, char *err, size_t errsz) {
    const char *s;
    long iv;
    double dv;

    memset(o, 0, sizeof(*o));
    o->classification = 'U';
    o->ephemeris_type = '0';
    o->element_set = 999;

    s = field_of(f, nf, c->epoch);
    if (!s) { snprintf(err, errsz, "缺少 EPOCH"); return 0; }
    if (strlen(s) >= sizeof(o->epoch)) { snprintf(err, errsz, "EPOCH 过长"); return 0; }
    strcpy(o->epoch, s);

    s = field_of(f, nf, c->mean_motion);
    if (!s || !parse_dbl(s, &dv)) { snprintf(err, errsz, "MEAN_MOTION 缺失或非法"); return 0; }
    o->mean_motion = dv;

    s = field_of(f, nf, c->eccentricity);
    if (!s || !parse_dbl(s, &dv)) { snprintf(err, errsz, "ECCENTRICITY 缺失或非法"); return 0; }
    o->eccentricity = dv;

    s = field_of(f, nf, c->inclination);
    if (!s || !parse_dbl(s, &dv)) { snprintf(err, errsz, "INCLINATION 缺失或非法"); return 0; }
    o->inclination = dv;

    s = field_of(f, nf, c->raan);
    if (!s || !parse_dbl(s, &dv)) { snprintf(err, errsz, "RA_OF_ASC_NODE 缺失或非法"); return 0; }
    o->raan = dv;

    s = field_of(f, nf, c->argp);
    if (!s || !parse_dbl(s, &dv)) { snprintf(err, errsz, "ARG_OF_PERICENTER 缺失或非法"); return 0; }
    o->argp = dv;

    s = field_of(f, nf, c->mean_anomaly);
    if (!s || !parse_dbl(s, &dv)) { snprintf(err, errsz, "MEAN_ANOMALY 缺失或非法"); return 0; }
    o->mean_anomaly = dv;

    s = field_of(f, nf, c->norad_id);
    if (!s || !parse_int(s, &iv)) { snprintf(err, errsz, "NORAD_CAT_ID 缺失或非法"); return 0; }
    o->norad_id = iv;

    /* ---- 以下可选, 有默认值 ---- */
    s = field_of(f, nf, c->object_name);
    if (s) {
        strncpy(o->object_name, s, OMM_NAME_BUF - 1);
        o->object_name[OMM_NAME_BUF - 1] = '\0';
    }

    s = field_of(f, nf, c->object_id);
    if (s) {
        strncpy(o->object_id, s, sizeof(o->object_id) - 1);
        o->object_id[sizeof(o->object_id) - 1] = '\0';
    }

    s = field_of(f, nf, c->classification);
    if (s) o->classification = s[0];

    s = field_of(f, nf, c->ephemeris_type);
    if (s && isdigit((unsigned char)s[0])) o->ephemeris_type = s[0];

    s = field_of(f, nf, c->element_set);
    if (s && parse_int(s, &iv)) o->element_set = iv;

    s = field_of(f, nf, c->rev);
    if (s && parse_dbl(s, &dv)) o->rev = (long)dv;      /* 小数截断 */

    s = field_of(f, nf, c->bstar);
    if (s && parse_dbl(s, &dv)) o->bstar = dv;

    s = field_of(f, nf, c->ndot);
    if (s && parse_dbl(s, &dv)) o->ndot = dv;

    s = field_of(f, nf, c->nddot);
    if (s && parse_dbl(s, &dv)) o->nddot = dv;

    s = field_of(f, nf, c->decayed);
    if (s && parse_int(s, &iv)) o->decayed = (iv != 0);

    return 1;
}

/* ------------------------- TLE 格式化 ------------------------- */

/*
 * OMM 的 OBJECT_ID "1998-067A" -> TLE 国际编号 "98067A  " (8 字符, 左对齐)。
 * 解析失败输出 8 个空格。
 */
static void format_object_id(const char *id, char out[9]) {
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
static int format_epoch(const char *iso, char out[15]) {
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
    while (av < 0.1)  { av *= 10.0; exp--; }
    if (exp < -9) { strcpy(out, " 00000+0"); return; }      /* 过小按 0 */
    if (exp > 9)  { snprintf(out, 9, "%c99999+9", sign); return; }
    digits = i_round(av * 1e5);
    if (digits >= 100000L) { digits = 10000L; exp++; }      /* 尾数进位 */
    if (exp > 9) { snprintf(out, 9, "%c99999+9", sign); return; }
    if (exp >= 0) snprintf(out, 9, "%c%05ld+%d", sign, digits, exp);
    else          snprintf(out, 9, "%c%05ld-%d", sign, digits, -exp);
}

/* TLE 校验和: 对前 68 列, 数字按值, 减号计 1, 其它计 0, 模 10 */
static int tle_checksum(const char *s) {
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
static void build_lines(const omm_t *o, int raw_ndot, int raw_nddot,
                        char *l0, char *l1, char *l2) {
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
    if (elem > 9999) { fprintf(stderr, "警告: 元素集号 %ld 超出 4 位, 截断\n", elem); elem = 9999; }
    if (rev < 0) { fprintf(stderr, "警告: 圈数 %ld 为负, 置 0\n", rev); rev = 0; }
    if (rev > 99999) { fprintf(stderr, "警告: 圈数 %ld 超出 5 位, 截断\n", rev); rev = 99999; }
    if (mm < 0.0) { fprintf(stderr, "警告: 平均运动为负, 置 0\n"); mm = 0.0; }
    if (mm >= 100.0) { fprintf(stderr, "警告: 平均运动 %.6f 超出 TLE 表示范围, 截断\n", mm); mm = 99.99999999; }
    ecci = i_round(o->eccentricity * 1e7);
    if (o->eccentricity < 0.0 || o->eccentricity >= 1.0) {
        fprintf(stderr, "警告: 偏心率 %.8f 超出 [0,1), 截断\n", o->eccentricity);
        if (o->eccentricity < 0.0) ecci = 0; else ecci = 9999999;
    }
    if (ecci < 0) ecci = 0;
    if (ecci > 9999999) ecci = 9999999;

    /* 第 0 行: 卫星名称*/
    if (o->object_name[0]) {
        snprintf(name, OMM_NAME_BUF, "%s", o->object_name);
    } else {
        snprintf(name, OMM_NAME_BUF, "%05ld", norad);   /* 无名称时用目录号 */
    }
    snprintf(l0, OMM_NAME_BUF, "%-0.32s", name);

    /* 第 1 行 */
    format_object_id(o->object_id, intl);
    if (!format_epoch(o->epoch, epoch)) {
        /* 解析阶段已校验, 此处不应发生 */
        snprintf(epoch, sizeof(epoch), "00000.00000000");
    }
    format_ndot(ndot, ndotf);
    format_exp(nddot, nddotf);
    format_exp(o->bstar, bstarf);
    snprintf(l1, OMM_TLE_BUF,
             "1 %05ld%c %-8s %14s %10s %8s %8s %c %4ld",
             norad, o->classification, intl, epoch, ndotf, nddotf, bstarf,
             o->ephemeris_type, elem);
    l1[68] = (char)('0' + tle_checksum(l1));
    l1[69] = '\0';

    /* 第 2 行 */
    snprintf(l2, OMM_TLE_BUF,
             "2 %05ld %8.4f %8.4f %07ld %8.4f %8.4f %11.8f%5ld",
             norad, incl, raan, ecci, argp, ma, mm, rev);
    l2[68] = (char)('0' + tle_checksum(l2));
    l2[69] = '\0';
}

/* ------------------------- 公开 API ------------------------- */

static int cfg_half_ndot = 0;       /* 1 = MEAN_MOTION_DOT 除以 2 */
static int cfg_sixth_nddot = 0;     /* 1 = MEAN_MOTION_DDOT 除以 6 */
static int cfg_skip_decayed = 0;    /* 1 = 跳过 DECAYED=1 */
static int cfg_check = 0;           /* 1 = 与 CSV 自带 TLE 参考行比对 */

static int stat_skipped = 0;        /* 跳过的记录数 */

/* 输入流状态(文件作用域, 便于 omm_reset 重置) */
static int  header_done = 0;        /* 是否已读取表头 */
static cols_t cols;                 /* 当前流的列映射 */
static char linebuf[LINE_BUF];      /* 行缓冲 */
static long stat_rec = 0;           /* 数据记录计数 */
omm_t SatData;

void omm_set_options(int half_ndot, int sixth_nddot, int skip_decayed, int check) {
    cfg_half_ndot = half_ndot ? 1 : 0;
    cfg_sixth_nddot = sixth_nddot ? 1 : 0;
    cfg_skip_decayed = skip_decayed ? 1 : 0;
    cfg_check = check ? 1 : 0;
}

int omm_get_skipped(void) {
    return stat_skipped;
}

void omm_reset(void) {
    /* 重置输入流状态(选项保持不变), 便于开始处理新的输入流 */
    header_done = 0;
    stat_rec = 0;
    stat_skipped = 0;
}

/* 从流读取一行(含换行); 行超长时丢弃剩余部分并警告。返回 1 成功 / 0 EOF。 */
static int read_next_line(FILE *fp, char *buf, size_t bufsz) {
    if (!fgets(buf, (int)bufsz, fp)) return 0;
    if (!strchr(buf, '\n') && !feof(fp)) {
        fprintf(stderr, "警告: 行超过 %u 字符, 已丢弃\n", (unsigned)bufsz);
        while (fgets(buf, (int)bufsz, fp)) {
            if (strchr(buf, '\n')) break;
        }
    }
    return 1;
}

/*
 * 核心转换函数: 从文件流读取一条 OMM 记录, 转换为 TLE 三行。
 * 首次调用时读取表头并建立列映射; 之后每次调用处理一条记录。
 * 返回: 1 = 成功; 0 = 文件结束; -1 = 致命错误(表头缺少必要列)。
 */
int omm_next_tle(FILE *fp, char *name, char *line1, char *line2) {
    char **hdr = NULL;
    int nhdr = 0;

    if (!fp || !name || !line1 || !line2) return -1;

    /* --- 首次调用: 读取表头, 建立列映射 --- */
    if (!header_done) {
        csvr_t r;
        if (!read_next_line(fp, linebuf, sizeof(linebuf))) return 0;   /* 空输入 */

        /* 跳过 UTF-8 BOM */
        if ((unsigned char)linebuf[0] == 0xEF && (unsigned char)linebuf[1] == 0xBB &&
            (unsigned char)linebuf[2] == 0xBF) {
            memmove(linebuf, linebuf + 3, strlen(linebuf) - 2);
        }

        r.cur = linebuf;
        r.end = linebuf + strlen(linebuf);
        if (!csv_next_record(&r, &hdr, &nhdr)) {
            fprintf(stderr, "错误: 无法解析表头\n");
            return -1;
        }
        init_cols(&cols);
        cols.object_name    = find_col(hdr, nhdr, "OBJECT_NAME");
        cols.object_id      = find_col(hdr, nhdr, "OBJECT_ID");
        cols.epoch          = find_col(hdr, nhdr, "EPOCH");
        cols.mean_motion    = find_col(hdr, nhdr, "MEAN_MOTION");
        cols.eccentricity   = find_col(hdr, nhdr, "ECCENTRICITY");
        cols.inclination    = find_col(hdr, nhdr, "INCLINATION");
        cols.raan           = find_col(hdr, nhdr, "RA_OF_ASC_NODE");
        cols.argp           = find_col(hdr, nhdr, "ARG_OF_PERICENTER");
        cols.mean_anomaly   = find_col(hdr, nhdr, "MEAN_ANOMALY");
        cols.ephemeris_type = find_col(hdr, nhdr, "EPHEMERIS_TYPE");
        cols.classification = find_col(hdr, nhdr, "CLASSIFICATION_TYPE");
        cols.norad_id       = find_col(hdr, nhdr, "NORAD_CAT_ID");
        cols.element_set    = find_col(hdr, nhdr, "ELEMENT_SET_NO");
        cols.rev            = find_col(hdr, nhdr, "REV_AT_EPOCH");
        cols.bstar          = find_col(hdr, nhdr, "BSTAR");
        cols.ndot           = find_col(hdr, nhdr, "MEAN_MOTION_DOT");
        cols.nddot          = find_col(hdr, nhdr, "MEAN_MOTION_DDOT");
        cols.decayed        = find_col(hdr, nhdr, "DECAYED");
        cols.tle0           = find_col(hdr, nhdr, "TLE_LINE0");
        cols.tle1           = find_col(hdr, nhdr, "TLE_LINE1");
        cols.tle2           = find_col(hdr, nhdr, "TLE_LINE2");
        free_csv_fields(hdr, nhdr);

        if (cols.epoch < 0 || cols.mean_motion < 0 || cols.eccentricity < 0 ||
            cols.inclination < 0 || cols.raan < 0 || cols.argp < 0 ||
            cols.mean_anomaly < 0 || cols.norad_id < 0) {
            fprintf(stderr, "错误: 表头缺少必要列(EPOCH/MEAN_MOTION/ECCENTRICITY/"
                            "INCLINATION/RA_OF_ASC_NODE/ARG_OF_PERICENTER/"
                            "MEAN_ANOMALY/NORAD_CAT_ID)\n");
            return -1;
        }

        header_done = 1;
    }
    /* --- 逐条处理数据记录 --- */
    for (;;) {
        csvr_t r;
        char **f = NULL;
        int nf = 0;
        omm_t o;
        char err[128];

        if (!read_next_line(fp, linebuf, sizeof(linebuf))) return 0;   /* EOF */
        stat_rec++;

        /* 空行跳过 */
        if (linebuf[0] == '\n' || linebuf[0] == '\r' || linebuf[0] == '\0') continue;

        r.cur = linebuf;
        r.end = linebuf + strlen(linebuf);
        if (!csv_next_record(&r, &f, &nf)) continue;

        if (!parse_row(f, nf, &cols, &o, err, sizeof(err))) {
            fprintf(stderr, "警告: 第 %ld 条记录跳过: %s\n", stat_rec, err);
            stat_skipped++;
            free_csv_fields(f, nf);
            continue;
        }
        if (cfg_skip_decayed && o.decayed) {
            stat_skipped++;
            free_csv_fields(f, nf);
            continue;
        }

        build_lines(&o, cfg_half_ndot ? 0 : 1, cfg_sixth_nddot ? 0 : 1,
                    name, line1, line2);

        free_csv_fields(f, nf);

        memcpy_s(&SatData,sizeof(omm_t),&o,sizeof(omm_t));
        return 1;
    }
}

void omm_get_object_data(omm_t* data) {
    memcpy_s(data,sizeof(omm_t),&SatData,sizeof(omm_t));
}
