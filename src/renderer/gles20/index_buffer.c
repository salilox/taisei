/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2019, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2019, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include "index_buffer.h"

IndexBuffer* gles20_index_buffer_create(size_t max_elements) {
	IndexBuffer *ibuf = calloc(1, sizeof(*ibuf) + max_elements * sizeof(gles20_ibo_index_t));
	snprintf(ibuf->debug_label, sizeof(ibuf->debug_label), "Fake IBO at %p", (void*)ibuf);
	ibuf->num_elements = max_elements;
	return ibuf;
}

size_t gles20_index_buffer_get_capacity(IndexBuffer *ibuf) {
	return ibuf->num_elements;
}

const char* gles20_index_buffer_get_debug_label(IndexBuffer *ibuf) {
	return ibuf->debug_label;
}

void gles20_index_buffer_set_debug_label(IndexBuffer *ibuf, const char *label) {
	if(label) {
		strlcpy(ibuf->debug_label, label, sizeof(ibuf->debug_label));
	} else {
		snprintf(ibuf->debug_label, sizeof(ibuf->debug_label), "Fake IBO at %p", (void*)ibuf);
	}
}

void gles20_index_buffer_set_offset(IndexBuffer *ibuf, size_t offset) {
	ibuf->offset = offset;
}

size_t gles20_index_buffer_get_offset(IndexBuffer *ibuf) {
	return ibuf->offset;
}

void gles20_index_buffer_add_indices(IndexBuffer *ibuf, uint index_ofs, size_t num_indices, uint indices[num_indices]) {
	assert(ibuf->offset + num_indices - 1 < ibuf->num_elements);

	for(size_t i = 0; i < num_indices; ++i) {
		uintmax_t idx = indices[i] + index_ofs;
		assert(idx <= GLES20_IBO_MAX_INDEX);
		ibuf->elements[ibuf->offset + i] = idx;
	}

	ibuf->offset += num_indices;
}

void gles20_index_buffer_destroy(IndexBuffer *ibuf) {
	free(ibuf);
}

void gles20_index_buffer_flush(IndexBuffer *ibuf) {
}
