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
#include <wchar.h>
#include "../omm2tle.h"
#include "../tool/string_operation.h"

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
} cols_t;


/* CSV 解析游标(指向一行内存缓冲) */
typedef struct {
    const char* cur;
    const char* end;
} csvr_t;

/* ------------------------- CSV 解析 ------------------------- */

static void free_csv_fields(char** f, int n) {
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
static int csv_next_record(csvr_t* r, char*** fields, int* nfields) {
    char** arr = NULL;
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
        }
        else {
            while (r->cur < r->end && *r->cur != ',' && *r->cur != '\n' && *r->cur != '\r') {
                if (len + 1 < FIELD_BUF) buf[len++] = *r->cur;
                r->cur++;
            }
        }
        buf[len] = '\0';
        trim(buf);

        /* --- 加入字段数组 --- */
        if (n == cap) {
            char** na;
            cap = cap ? cap * 2 : 8;
            na = (char**)realloc(arr, (size_t)cap * sizeof(char*));
            if (!na) {
                fwprintf(stderr, L"错误: 内存不足\n");
                free_csv_fields(arr, n);
                exit(1);
            }
            arr = na;
        }
        arr[n] = (char*)malloc(strlen(buf) + 1);
        if (!arr[n]) {
            fwprintf(stderr, L"错误: 内存不足\n");
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
static int find_col(char** hdr, int nhdr, const char* name) {
    int i;
    for (i = 0; i < nhdr; i++) {
        if (name_eq(hdr[i], name)) return i;
    }
    return -1;
}

static void init_cols(cols_t* c) {
    c->object_name = c->object_id = c->epoch = -1;
    c->mean_motion = c->eccentricity = c->inclination = c->raan = c->argp = -1;
    c->mean_anomaly = c->ephemeris_type = c->classification = c->norad_id = -1;
    c->element_set = c->rev = c->bstar = c->ndot = c->nddot = c->decayed = -1;
}

/* 取第 idx 列的内容; 列不存在或为空返回 NULL */
static const char* field_of(char** field, int nf, int idx) {
    if (idx < 0 || idx >= nf) return NULL;
    if (!field[idx] || !field[idx][0]) return NULL;
    return field[idx];
}


/* ------------------------- 公开 API ------------------------- */

static int cfg_half_ndot = 0;       /* 1 = MEAN_MOTION_DOT 除以 2 */
static int cfg_sixth_nddot = 0;     /* 1 = MEAN_MOTION_DDOT 除以 6 */
static int cfg_skip_decayed = 0;    /* 1 = 跳过 DECAYED=1 */

static int stat_skipped = 0;        /* 跳过的记录数 */

/* 输入流状态(文件作用域, 便于 omm_reset 重置) */
static int  header_done = 0;        /* 是否已读取表头 */
static cols_t cols;                 /* 当前流的列映射 */
static char linebuf[LINE_BUF];      /* 行缓冲 */
static long stat_rec = 0;           /* 数据记录计数 */




void csv_reset(void) {
    /* 重置输入流状态(选项保持不变), 便于开始处理新的输入流 */
    header_done = 0;
    stat_rec = 0;
    stat_skipped = 0;
}

/* ------------------------- 解析一行 OMM ------------------------- */

static int parse_row(char** f, int nf, const cols_t* c, omm_t* o, char* err, size_t errsz) {
    const char* s;
    long iv;
    double dv;

    memset(o, 0, sizeof(*o));
    o->classification = 'U';
    o->ephemeris_type = '0';
    o->element_set = 999;

    s = field_of(f, nf, c->epoch);
    if (!s) { snprintf(err, errsz, "缺少 EPOCH"); return 0; }
    if (strlen(s) >= sizeof(o->epoch)) { swprintf(err, errsz, L"EPOCH 过长"); return 0; }
    strcpy(o->epoch, s);

    s = field_of(f, nf, c->mean_motion);
    if (!s || !parse_dbl(s, &dv)) { swprintf(err, errsz, L"MEAN_MOTION 缺失或非法"); return 0; }
    o->mean_motion = dv;

    s = field_of(f, nf, c->eccentricity);
    if (!s || !parse_dbl(s, &dv)) { swprintf(err, errsz, L"ECCENTRICITY 缺失或非法"); return 0; }
    o->eccentricity = dv;

    s = field_of(f, nf, c->inclination);
    if (!s || !parse_dbl(s, &dv)) { swprintf(err, errsz, L"LINCLINATION 缺失或非法"); return 0; }
    o->inclination = dv;

    s = field_of(f, nf, c->raan);
    if (!s || !parse_dbl(s, &dv)) { swprintf(err, errsz, L"RA_OF_ASC_NODE 缺失或非法"); return 0; }
    o->raan = dv;

    s = field_of(f, nf, c->argp);
    if (!s || !parse_dbl(s, &dv)) { swprintf(err, errsz, L"ARG_OF_PERICENTER 缺失或非法"); return 0; }
    o->argp = dv;

    s = field_of(f, nf, c->mean_anomaly);
    if (!s || !parse_dbl(s, &dv)) { swprintf(err, errsz, L"MEAN_ANOMALY 缺失或非法"); return 0; }
    o->mean_anomaly = dv;

    s = field_of(f, nf, c->norad_id);
    if (!s || !parse_uint(s, &iv)) { swprintf(err, errsz, L"NORAD_CAT_ID 缺失或非法"); return 0; }
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

/* 从流读取一行(含换行); 行超长时丢弃剩余部分并警告。返回 1 成功 / 0 EOF。 */
static int file_next_line(FILE* fp, char* buf, size_t bufsz) {
    if (!fgets(buf, (int)bufsz, fp)) return 0;
    if (!strchr(buf, '\n') && !feof(fp)) {
        fwprintf(stderr, L"警告: 行超过 %u 字符, 已丢弃\n", (unsigned)bufsz);
        while (fgets(buf, (int)bufsz, fp)) {
            if (strchr(buf, '\n')) break;
        }
    }
    return 1;
}


int csv_next_line(FILE* fp, omm_t* data) {
    char** hdr = NULL;
    int nhdr = 0;

    /* --- 首次调用: 读取表头, 建立列映射 --- */
    if (!header_done) {
        csvr_t r;
        if (!file_next_line(fp, linebuf, sizeof(linebuf))) return 0;   /* 空输入 */

        /* 跳过 UTF-8 BOM */
        if ((unsigned char)linebuf[0] == 0xEF && (unsigned char)linebuf[1] == 0xBB &&
            (unsigned char)linebuf[2] == 0xBF) {
            memmove(linebuf, linebuf + 3, strlen(linebuf) - 2);
        }

        r.cur = linebuf;
        r.end = linebuf + strlen(linebuf);
        if (!csv_next_record(&r, &hdr, &nhdr)) {
            fwprintf(stderr, L"错误: 无法解析表头\n");
            return -1;
        }
        init_cols(&cols);
        cols.object_name = find_col(hdr, nhdr, "OBJECT_NAME");
        cols.object_id = find_col(hdr, nhdr, "OBJECT_ID");
        cols.epoch = find_col(hdr, nhdr, "EPOCH");
        cols.mean_motion = find_col(hdr, nhdr, "MEAN_MOTION");
        cols.eccentricity = find_col(hdr, nhdr, "ECCENTRICITY");
        cols.inclination = find_col(hdr, nhdr, "INCLINATION");
        cols.raan = find_col(hdr, nhdr, "RA_OF_ASC_NODE");
        cols.argp = find_col(hdr, nhdr, "ARG_OF_PERICENTER");
        cols.mean_anomaly = find_col(hdr, nhdr, "MEAN_ANOMALY");
        cols.ephemeris_type = find_col(hdr, nhdr, "EPHEMERIS_TYPE");
        cols.classification = find_col(hdr, nhdr, "CLASSIFICATION_TYPE");
        cols.norad_id = find_col(hdr, nhdr, "NORAD_CAT_ID");
        cols.element_set = find_col(hdr, nhdr, "ELEMENT_SET_NO");
        cols.rev = find_col(hdr, nhdr, "REV_AT_EPOCH");
        cols.bstar = find_col(hdr, nhdr, "BSTAR");
        cols.ndot = find_col(hdr, nhdr, "MEAN_MOTION_DOT");
        cols.nddot = find_col(hdr, nhdr, "MEAN_MOTION_DDOT");
        cols.decayed = find_col(hdr, nhdr, "DECAYED");
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
        char** f = NULL;
        int nf = 0;
        omm_t o;
        char err[128];

        if (!file_next_line(fp, linebuf, sizeof(linebuf))) return 0;   /* EOF */
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

        memcpy(data, &o, sizeof(omm_t));

        free_csv_fields(f, nf);


        return 1;
    }
}