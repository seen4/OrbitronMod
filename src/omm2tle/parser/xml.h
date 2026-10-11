#pragma once
#include<stdio.h>
#include "../omm2tle.h"

#ifdef _cpluspus
extern "C" {
#endif
	extern void xml_reset();
	extern _Bool xml_detect(FILE* fp);
	extern int xml_parse(FILE* fp, omm_t* data);
#ifdef _cpluspus
}
#endif

