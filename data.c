#include "data.h"


size_t sat_push(sat_data* self, omm_t data) {
	if (self->used >= self->capacity) {
		if(!self->data) return -1;
		self->data = (omm_t**)realloc(self->data, (self->capacity + 1) * sizeof(omm_t*));
		if (!self->data) {
			return -1;
		}
		self->capacity++;
	}
	self->used++;
	self->data[self->used-1] = (omm_t*)malloc(sizeof(omm_t));
	memcpy_s(self->data[self->used-1], sizeof(omm_t), &data, sizeof(omm_t));
	return self->used;
}

void sat_access(sat_data self, omm_t* data, size_t id) {
	memcpy_s(data, sizeof(omm_t), self.data[id-1] , sizeof(omm_t));
}

sat_data sat_create() {
	sat_data d;

	d.data = (omm_t**)malloc(SAT_DATA_DEFAULT_CAPACITY * sizeof(omm_t*));
	if (!d.data) {
		return d;
	}
	d.capacity = SAT_DATA_DEFAULT_CAPACITY;

	return d;
}

void sat_destroy(sat_data data) {
	for (size_t i = 0; i < data.used; i++) {
		free(data.data[i]);
	}
	free(data.data);
}