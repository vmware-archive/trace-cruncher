// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

// KernelShark
#include "libkshark.h"

ssize_t trace2matrix(int sd,
		     int16_t **event_array,
		     int16_t **cpu_array,
		     int32_t **pid_array,
		     int64_t **offset_array,
		     int64_t **ts_array)
{
	struct kshark_generic_stream_interface *interface;
	struct kshark_context *kshark_ctx = NULL;
	struct kshark_data_stream *stream;
	ssize_t total = 0;

	if (!kshark_instance(&kshark_ctx))
		return -1;

	stream = kshark_get_data_stream(kshark_ctx, sd);
	if (!stream)
		return -1;

	interface = stream->interface;
	if (interface->type == KS_GENERIC_DATA_INTERFACE &&
	    interface->load_matrix) {
		total = interface->load_matrix(stream, kshark_ctx, event_array,
								   cpu_array,
								   pid_array,
								   offset_array,
								   ts_array);
	}

	return total;
}
