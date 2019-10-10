// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
 */

 /**
  *  @file    libkshark-py.c
  *  @brief   Python API for processing of FTRACE (trace-cmd) data.
  */

// KernelShark
#include "kernelshark/libkshark.h"
#include "kernelshark/libkshark-model.h"

bool kspy_open(const char *fname)
{
	struct kshark_context *kshark_ctx = NULL;

	if (!kshark_instance(&kshark_ctx))
		return false;

	return kshark_open(kshark_ctx, fname);
}

void kspy_close(void)
{
	struct kshark_context *kshark_ctx = NULL;

	if (!kshark_instance(&kshark_ctx))
		return;

	kshark_close(kshark_ctx);
	kshark_free(kshark_ctx);
}

static int compare(const void * a, const void * b)
{
	return *(int*)a - *(int*)b;
}

size_t kspy_get_tasks(int **pids, char ***names)
{
	struct kshark_context *kshark_ctx = NULL;
	const char *comm;
	ssize_t i, n;
	int ret;

	if (!kshark_instance(&kshark_ctx))
		return 0;

	n = kshark_get_task_pids(kshark_ctx, pids);
	if (n == 0)
		return 0;

	qsort(*pids, n, sizeof(**pids), compare);

	*names = calloc(n, sizeof(char*));
	if (!(*names))
		goto fail;

	for (i = 0; i < n; ++i) {
		comm = tep_data_comm_from_pid(kshark_ctx->pevent, (*pids)[i]);
		if (i == 191)
			printf("pid: %i  comm: %s\n", (*pids)[i], comm);
		ret = asprintf(&(*names)[i], "%s", comm);
		if (ret < 1)
			goto fail;
	}

	return n;

  fail:
	free(*pids);
	free(*names);
	return 0;
}

size_t kspy_trace2matrix(uint64_t **offset_array,
			 uint16_t **cpu_array,
			 uint64_t **ts_array,
			 uint16_t **pid_array,
			 int **event_array)
{
	struct kshark_context *kshark_ctx = NULL;
	size_t total = 0;

	if (!kshark_instance(&kshark_ctx))
		return false;

	total = kshark_load_data_matrix(kshark_ctx, offset_array,
					cpu_array,
					ts_array,
					pid_array,
					event_array);

	return total;
}

int kspy_get_event_id(const char *sys, const char *evt)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tep_event *event;

	if (!kshark_instance(&kshark_ctx))
		return -1;

	event = tep_find_event_by_name(kshark_ctx->pevent, sys, evt);

	return event->id;
}

unsigned long long kspy_read_event_field(uint64_t offset,
					 int id, const char *field)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tep_format_field *evt_field;
	struct tep_record *record;
	struct tep_event *event;
	unsigned long long val;
	int ret;

	if (!kshark_instance(&kshark_ctx))
		return 0;

	event = tep_find_event(kshark_ctx->pevent, id);
	if (!event)
		return 0;

	evt_field = tep_find_any_field(event, field);
	if (!evt_field)
		return 0;

	record = tracecmd_read_at(kshark_ctx->handle, offset, NULL);
	if (!record)
		return 0;

	ret = tep_read_number_field(evt_field, record->data, &val);
	free_record(record);

	if (ret != 0)
		return 0;

	return val;
}

const char *kspy_get_function(unsigned long long addr)
{
	struct kshark_context *kshark_ctx = NULL;

	if (!kshark_instance(&kshark_ctx))
		return "";

	return tep_find_function(kshark_ctx->pevent, addr);
}

void kspy_register_plugin(const char *plugin)
{
	struct kshark_context *kshark_ctx = NULL;
	char *lib_file;
	int n;

	if (!kshark_instance(&kshark_ctx))
		return;

	n = asprintf(&lib_file, "%s/plugin-%s.so", KS_PLUGIN_DIR, plugin);
	if (n > 0) {
		kshark_register_plugin(kshark_ctx, lib_file);
		kshark_handle_plugins(kshark_ctx, KSHARK_PLUGIN_INIT);
		free(lib_file);
	}
}

const char *kspy_map_instruction_address(int pid, unsigned long long proc_addr,
					 unsigned long long *obj_addr)
{
	struct kshark_context *kshark_ctx = NULL;
	struct tracecmd_proc_addr_map *mem_map;

	*obj_addr = 0;
	if (!kshark_instance(&kshark_ctx))
		return "UNKNOWN";

	mem_map = tracecmd_search_task_map(kshark_ctx->handle,
					   pid, proc_addr);

	if (!mem_map)
		return "UNKNOWN";

	*obj_addr = proc_addr - mem_map->start;

	return mem_map->lib_name;
}

void kspy_new_session_file(const char *data_file, const char *session_file)
{
	struct kshark_context *kshark_ctx = NULL;
	struct kshark_trace_histo histo;
	struct kshark_config_doc *session;
	struct kshark_config_doc *filters;
	struct kshark_config_doc *markers;
	struct kshark_config_doc *model;
	struct kshark_config_doc *file;

	if (!kshark_instance(&kshark_ctx))
		return;

	session = kshark_config_new("kshark.config.session",
				    KS_CONFIG_JSON);

	file = kshark_export_trace_file(data_file, KS_CONFIG_JSON);
	kshark_config_doc_add(session, "Data", file);

	filters = kshark_export_all_filters(kshark_ctx, KS_CONFIG_JSON);
	kshark_config_doc_add(session, "Filters", filters);

	ksmodel_init(&histo);
	model = kshark_export_model(&histo, KS_CONFIG_JSON);
	kshark_config_doc_add(session, "Model", model);

	markers = kshark_config_new("kshark.config.markers", KS_CONFIG_JSON);
	kshark_config_doc_add(session, "Markers", markers);

	kshark_save_config_file(session_file, session);
	kshark_free_config_doc(session);
}
