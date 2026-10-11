#pragma once

#include <stdio.h>
#include "../omm2tle.h"

extern int csv_next_line(FILE* fp, omm_t* data);
extern void csv_reset();