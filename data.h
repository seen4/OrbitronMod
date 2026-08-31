#pragma once

#include "omm2tle.h"
#include <stdlib.h>
#include <memory.h>

#define SAT_DATA_DEFAULT_CAPACITY 256

typedef struct {
	size_t capacity;
	size_t used;
	omm_t **data;
}sat_data;

#ifdef __cplusplus
extern "C" {
#endif

extern size_t sat_push(sat_data* self, omm_t data);

extern void sat_access(sat_data self, omm_t* data, size_t id);
 
extern sat_data sat_create();

extern void sat_clear(sat_data* self);

extern void sat_destroy(sat_data *data);

#ifdef __cplusplus
}
#endif