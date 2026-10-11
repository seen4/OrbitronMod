#pragma once

#ifdef __cplusplus
extern "C"{
#endif // cplusplus

extern long i_round(double x);
extern int is_leap(int y);
extern int days_in_month(int y, int m);
extern int day_of_year(int y, int m, int d);
extern int name_eq(const char* a, const char* b);
extern void trim(char* s);
extern int parse_int(const char* s, long* out);
extern int parse_uint(const char* s, long* out);
extern int parse_dbl(const char* s, double* out);

#ifdef __cplusplus
}
#endif // cplusplus

