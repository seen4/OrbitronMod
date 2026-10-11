#pragma once
#include "../omm2tle.h"

#ifdef _cpluspus
extern "C"{
#endif
extern void kvn_reset();
extern _Bool kvn_detect(FILE* fp);
extern int kvn_parse(FILE* fp, omm_t* data);
#ifdef _cpluspus
}
#endif
