// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

// C
#include <string.h>

// KernelShark
#include "libkshark.h"
#include "libkshark-plugin.h"
#include "libkshark-model.h"
#include "libkshark-tepdata.h"

// trace-cruncher
#include "ksharkpy-utils.h"

PyObject *KSHARK_ERROR = NULL;
PyObject *TRACECRUNCHER_ERROR = NULL;

PyObject *PyKShark_open(PyObject *self, PyObject *args, PyObject *kwargs)
{
	struct kshark_context *kshark_ctx = NULL;
	char *fname;
	int sd;

	static char *kwlist[] = {"file_name", NULL};
	if(!PyArg_ParseTupleAndKeywords(args,
					kwargs,
					"s",
					kwlist,
					&fname)) {
		return NULL;
	}

	if (!kshark_instance(&kshark_ctx)) {
		KS_INIT_ERROR
		return NULL;
	}

	sd = kshark_open(kshark_ctx, fname);
	if (sd < 0) {
		PyErr_Format(KSHARK_ERROR, "Failed to open file \'%s\'", fname);
		return NULL;
	}

	return PyLong_FromLong(sd);
}

PyObject *PyKShark_close(PyObject* self, PyObject* noarg)
{
	struct kshark_context *kshark_ctx = NULL;

	if (!kshark_instance(&kshark_ctx)) {
		KS_INIT_ERROR
		return NULL;
	}

	kshark_close_all(kshark_ctx);

	Py_RETURN_NONE;
}

static bool is_tep_data(const char *file_name)
{
	if (!kshark_tep_check_data(file_name)) {
		PyErr_Format(KSHARK_ERROR, "\'%s\' is not a TEP data file.",
			     file_name);
		return false;
	}

	return true;
}

PyObject *PyKShark_open_tep_buffer(PyObject *self, PyObject *args,
						   PyObject *kwargs)
{
	struct kshark_context *kshark_ctx = NULL;
	char *file_name, *buffer_name;
	int sd, sd_top;

	static char *kwlist[] = {"file_name", "buffer_name", NULL};
	if(!PyArg_ParseTupleAndKeywords(args,
					kwargs,
					"ss",
					kwlist,
					&file_name,
					&buffer_name)) {
		return NULL;
	}

	if (!kshark_instance(&kshark_ctx)) {
		KS_INIT_ERROR
		return NULL;
	}

	if (!is_tep_data(file_name))
		return NULL;

	sd_top = kshark_tep_find_top_stream(kshark_ctx, file_name);
	if (sd_top < 0) {
		/* The "top" steam has to be initialized first. */
		sd_top = kshark_open(kshark_ctx, file_name);
	}

	if (sd_top < 0)
		return NULL;

	sd = kshark_tep_open_buffer(kshark_ctx, sd_top, buffer_name);
	if (sd < 0) {
		PyErr_Format(KSHARK_ERROR,
			     "Failed to open buffer \'%s\' in file \'%s\'",
			     buffer_name, file_name);
		return NULL;
	}

	return PyLong_FromLong(sd);
}

static struct kshark_data_stream *get_stream(int stream_id)
{
	struct kshark_context *kshark_ctx = NULL;
	struct kshark_data_stream *stream;

	if (!kshark_instance(&kshark_ctx)) {
		KS_INIT_ERROR
		return NULL;
	}

	stream = kshark_get_data_stream(kshark_ctx, stream_id);
	if (!stream) {
		PyErr_Format(KSHARK_ERROR,
			     "No data stream %i loaded.",
			     stream_id);
		return NULL;
	}

	return stream;
}

PyObject *PyKShark_set_clock_offset(PyObject* self, PyObject* args,
						    PyObject *kwargs)
{
	struct kshark_data_stream *stream;
	int64_t offset;
	int stream_id;

	static char *kwlist[] = {"stream_id", "offset", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "iL",
					 kwlist,
					 &stream_id,
					 &offset)) {
		return NULL;
	}

	stream = get_stream(stream_id);
	if (!stream)
		return NULL;

	if (stream->calib_array)
		free(stream->calib_array);

	stream->calib_array = malloc(sizeof(*stream->calib_array));
	if (!stream->calib_array) {
		MEM_ERROR
		return NULL;
	}

	stream->calib_array[0] = offset;
	stream->calib_array_size = 1;

	stream->calib = kshark_offset_calib;

	Py_RETURN_NONE;
}

static int compare(const void *a, const void *b)
{
	int a_i, b_i;

	a_i = *(const int *) a;
	b_i = *(const int *) b;

	if (a_i > b_i)
		return +1;

	if (a_i < b_i)
		return -1;

	return 0;
}

PyObject *PyKShark_get_tasks(PyObject* self, PyObject* args, PyObject *kwargs)
{
	struct kshark_context *kshark_ctx = NULL;
	const char *comm;
	int sd, *pids;
	ssize_t i, n;

	static char *kwlist[] = {"stream_id", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "i",
					 kwlist,
					 &sd)) {
		return NULL;
	}

	if (!kshark_instance(&kshark_ctx)) {
		KS_INIT_ERROR
		return NULL;
	}

	n = kshark_get_task_pids(kshark_ctx, sd, &pids);
	if (n <= 0) {
		PyErr_SetString(KSHARK_ERROR,
				"Failed to retrieve the PID-s of the tasks");
		return NULL;
	}

	qsort(pids, n, sizeof(*pids), compare);

	PyObject *tasks, *pid_list, *pid_val;

	tasks = PyDict_New();
	for (i = 0; i < n; ++i) {
		comm = kshark_comm_from_pid(sd, pids[i]);
		pid_val = PyLong_FromLong(pids[i]);
		pid_list = PyDict_GetItemString(tasks, comm);
		if (!pid_list) {
			pid_list = PyList_New(1);
			PyList_SET_ITEM(pid_list, 0, pid_val);
			PyDict_SetItemString(tasks, comm, pid_list);
		} else {
			PyList_Append(pid_list, pid_val);
		}
	}

	return tasks;
}

PyObject *PyKShark_event_id(PyObject *self, PyObject *args, PyObject *kwargs)
{
	struct kshark_data_stream *stream;
	int stream_id, event_id;
	const char *name;

	static char *kwlist[] = {"stream_id", "name", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "is",
					 kwlist,
					 &stream_id,
					 &name)) {
		return NULL;
	}

	stream = get_stream(stream_id);
	if (!stream)
		return NULL;

	event_id = kshark_find_event_id(stream, name);
	if (event_id < 0) {
		PyErr_Format(KSHARK_ERROR,
			     "Failed to retrieve the Id of event \'%s\' in stream \'%s\'",
			     name, stream->file);
		return NULL;
	}

	return PyLong_FromLong(event_id);
}

PyObject *PyKShark_event_name(PyObject *self, PyObject *args,
					      PyObject *kwargs)
{
	struct kshark_data_stream *stream;
	struct kshark_entry entry;
	int stream_id, event_id;
	PyObject *ret;
	char *name;

	static char *kwlist[] = {"stream_id", "event_id", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ii",
					 kwlist,
					 &stream_id,
					 &event_id)) {
		return NULL;
	}

	stream = get_stream(stream_id);
	if (!stream)
		return NULL;

	entry.event_id = event_id;
	entry.stream_id = stream_id;
	entry.visible = 0xFF;
	name = kshark_get_event_name(&entry);
	if (!name) {
		PyErr_Format(KSHARK_ERROR,
			     "Failed to retrieve the name of event \'id=%i\' in stream \'%s\'",
			     event_id, stream->file);
		return NULL;
	}

	ret = PyUnicode_FromString(name);
	free(name);

	return ret;
}

PyObject *PyKShark_read_event_field(PyObject *self, PyObject *args,
						    PyObject *kwargs)
{
	struct kshark_context *kshark_ctx = NULL;
	struct kshark_entry entry;
	int event_id, ret, sd;
	const char *field;
	int64_t offset;
	int64_t val;

	static char *kwlist[] = {"stream_id", "offset", "event_id", "field", NULL};
	if(!PyArg_ParseTupleAndKeywords(args,
					kwargs,
					"iLis",
					kwlist,
					&sd,
					&offset,
					&event_id,
					&field)) {
		return NULL;
	}

	if (!kshark_instance(&kshark_ctx)) {
		KS_INIT_ERROR
		return NULL;
	}

	entry.event_id = event_id;
	entry.offset = offset;
	entry.stream_id = sd;

	ret = kshark_read_event_field_int(&entry, field, &val);
	if (ret != 0) {
		PyErr_Format(KSHARK_ERROR,
			     "Failed to read field '%s' of event '%i'",
			     field, event_id);
		return NULL;
	}

	return PyLong_FromLong(val);
}

PyObject *PyKShark_new_session_file(PyObject *self, PyObject *args,
						    PyObject *kwargs)
{
	struct kshark_context *kshark_ctx = NULL;
	struct kshark_config_doc *session;
	struct kshark_config_doc *plugins;
	struct kshark_config_doc *markers;
	struct kshark_config_doc *model;
	struct kshark_trace_histo histo;
	const char *session_file;

	static char *kwlist[] = {"session_file", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s",
					 kwlist,
					 &session_file)) {
		return NULL;
	}

	if (!kshark_instance(&kshark_ctx)) {
		KS_INIT_ERROR
		return NULL;
	}

	session = kshark_config_new("kshark.config.session",
				    KS_CONFIG_JSON);

	kshark_ctx->filter_mask = KS_TEXT_VIEW_FILTER_MASK |
				  KS_GRAPH_VIEW_FILTER_MASK |
				  KS_EVENT_VIEW_FILTER_MASK;

	kshark_export_all_dstreams(kshark_ctx, &session);

	ksmodel_init(&histo);
	model = kshark_export_model(&histo, KS_CONFIG_JSON);
	kshark_config_doc_add(session, "Model", model);

	markers = kshark_config_new("kshark.config.markers", KS_CONFIG_JSON);
	kshark_config_doc_add(session, "Markers", markers);

	plugins = kshark_config_new("kshark.config.plugins", KS_CONFIG_JSON);
	kshark_config_doc_add(session, "User Plugins", plugins);

	kshark_save_config_file(session_file, session);
	kshark_free_config_doc(session);

	Py_RETURN_NONE;
}
