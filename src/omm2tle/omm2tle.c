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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "omm2tle.h"
#include "tle_builder.h"

#include "parser/csv.h"
#include "parser/kvn.h"
#include "parser/xml.h"


/* ------------------------- 公开 API ------------------------- */

static int cfg_half_ndot = 0;       /* 1 = MEAN_MOTION_DOT 除以 2 */
static int cfg_sixth_nddot = 0;     /* 1 = MEAN_MOTION_DDOT 除以 6 */
static int cfg_skip_decayed = 0;    /* 1 = 跳过 DECAYED=1 */
static int cfg_check = 0;           /* 1 = 与 CSV 自带 TLE 参考行比对 */

static int stat_skipped = 0;        /* 跳过的记录数 */

omm_parser parser;
omm_t SatData;



/*  初始化解析器  */
int omm_init(FILE *fp) {

    if (xml_detect(fp)) {
        /* 设置解析器类型 */
        parser.parser = PARSER_XML;
        /* 初始化函数指针 */
        parser.parse_next_line = xml_parse;
        parser.parser_reset = xml_reset;
        return 0;
    }
    if (kvn_detect(fp)) {
        /* 设置解析器类型 */
        parser.parser = PARSER_KVN;
        /* 初始化函数指针 */
        parser.parse_next_line = kvn_parse;
        parser.parser_reset = kvn_reset;
        return 0;
    }
    /* 设置解析器类型 */
    parser.parser = PARSER_CSV;   
    /* 初始化函数指针 */
    parser.parse_next_line = csv_next_line;
    parser.parser_reset = csv_reset;

    return 0;
}


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
    parser.parser_reset();
}

/*
 * 核心转换函数: 从文件流读取一条 OMM 记录, 转换为 TLE 三行。
 * 首次调用时读取表头并建立列映射; 之后每次调用处理一条记录。
 * 返回: 1 = 成功; 0 = 文件结束; -1 = 致命错误(表头缺少必要列)。
 */
int omm_next_tle(FILE *fp, char *name, char *line1, char *line2) {
    omm_t parsed_data;
    int status=0;

    if (!fp || !name || !line1 || !line2) return -1;


    status = parser.parse_next_line(fp,&parsed_data);
    if(status == 0 || status == -1) return status;

    build_lines(&parsed_data, cfg_half_ndot ? 0 : 1,
                cfg_sixth_nddot ? 0 : 1, name, line1, line2);
    memcpy_s(&SatData, sizeof(omm_t), &parsed_data, sizeof(omm_t));

    return status;
}

void omm_get_object_data(omm_t* data) {
    memcpy_s(data,sizeof(omm_t),&SatData,sizeof(omm_t));
}
