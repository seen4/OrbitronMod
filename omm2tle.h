#pragma once
/*
 * omm2tle.h — OMM(Orbit Mean-Element Message) CSV -> TLE 转换 API
 *
 * 输入格式参考: CelesTrak gp.php ... FORMAT=csv
 * 输出格式参考: projectpluto tle_info (https://www.projectpluto.com/tle_info.htm)
 *
 * 用法示例:
 *     FILE *fp = fopen("iss.omm", "r");
 *     char name[OMM_NAME_BUF], line1[OMM_TLE_BUF], line2[OMM_TLE_BUF];
 *     while (omm_next_tle(fp, name, line1, line2) > 0) {
 *         printf("%s\n%s\n%s\n", name, line1, line2);
 *     }
 */

#include <stdio.h>

#define OMM_NAME_BUF 33   /* 卫星名称缓冲区: 32 字符 + NUL */
#define OMM_TLE_BUF  70   /* TLE 行缓冲区: 69 字符 + NUL */

 /* 一颗卫星的 OMM 参数 */
typedef struct {
	char object_name[OMM_NAME_BUF];
	char object_id[32];
	char epoch[40];
	double mean_motion, eccentricity, inclination, raan, argp, mean_anomaly;
	double bstar, ndot, nddot;
	long   norad_id, element_set;
	long   rev;
	char   classification;
	char   ephemeris_type;
	int    decayed;
} omm_t;


#ifdef __cplusplus
extern "C" {

	/*
	 * 从文件流读取一条 OMM 记录并转换为 TLE 三行。
	 * 参数: fp      输入文件流(必须先定位到文件开头, 首次调用时读取表头)
	 *       name    输出: 卫星名称(TLE 第 0 行, 24 字符, 不足补空格)
	 *       line1   输出: TLE 第 1 行(69 字符, 含校验和)
	 *       line2   输出: TLE 第 2 行(69 字符, 含校验和)
	 * 缓冲区大小须 >= OMM_NAME_BUF / OMM_TLE_BUF。
	 * 返回: 1 = 成功; 0 = 已到文件末尾; -1 = 致命错误(表头缺少必要列等)。
	 * 注: 解析失败或 DECAYED=1(启用 skip_decayed 时)的记录自动跳过并继续。
	 */
	extern int omm_next_tle(FILE* fp, char* name, char* line1, char* line2);

	/*
	 * 设置转换选项(任一时间调用, 在下一次 omm_next_tle 前生效):
	 *   half_ndot     1 = 将 MEAN_MOTION_DOT 除以 2 后写入 TLE(严格 CCSDS 解释;
	 *                 默认 0, 即原样写入, 与 CelesTrak OMM CSV 一致)
	 *   sixth_nddot   1 = 将 MEAN_MOTION_DDOT 除以 6 后写入(默认 0)
	 *   skip_decayed  1 = 跳过 DECAYED=1 的卫星(默认 0)
	 *   check         1 = 若 CSV 含 TLE_LINE1/TLE_LINE2 列, 与生成结果逐行比对,
	 *                     不一致时打印到 stderr 并计数(默认 0)
	 */
	extern void omm_set_options(int half_ndot, int sixth_nddot, int skip_decayed, int check);

	/* 处理过程中被跳过的记录数(解析失败/已再入) */
	extern int omm_get_skipped(void);

	/* 重置内部状态(表头、计数等), 便于开始处理新的输入流 */
	extern void omm_reset(void);

	extern void omm_get_object_data(omm_t* data);

}

#endif // __STDC__


