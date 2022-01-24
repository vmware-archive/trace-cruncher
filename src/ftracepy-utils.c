// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

// C
#include <search.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

// trace-cruncher
#include "ftracepy-utils.h"

PyObject *TFS_ERROR;
PyObject *TEP_ERROR;
PyObject *TRACECRUNCHER_ERROR;

static const char *get_instance_name(struct tracefs_instance *instance);

static char *tfs_error_log(struct tracefs_instance *instance, bool *ok)
{;
	char *err_log;

	errno = 0;
	err_log = tracefs_error_all(instance);
	if (errno && !err_log) {
		PyErr_Format(TFS_ERROR,
			     "Unable to get error log for instance \'%s\'.",
		             get_instance_name(instance));
	}

	if (ok)
		*ok = errno ? false : true;

	return err_log;
}

static bool tfs_clear_error_log(struct tracefs_instance *instance)
{
	if (tracefs_error_clear(instance) < 0) {
		PyErr_Format(TFS_ERROR,
			     "Unable to clear error log for instance \'%s\'.",
			     get_instance_name(instance));
		return false;
	}

	return true;
}

static void TfsError_fmt(struct tracefs_instance *instance,
			 const char *fmt, ...)
{
	char *tfs_err_log = tfs_error_log(instance, NULL);
	va_list args;

	va_start(args, fmt);
	if (tfs_err_log) {
		char *tc_err_log;

		vasprintf(&tc_err_log, fmt, args);
		va_end(args);

		PyErr_Format(TFS_ERROR, "%s\ntfs_error: %s",
			     tc_err_log, tfs_err_log);

		tfs_clear_error_log(instance);
		free(tfs_err_log);
		free(tc_err_log);
	} else {
		PyErr_FormatV(TFS_ERROR, fmt, args);
		va_end(args);
	}
}

static void TfsError_setstr(struct tracefs_instance *instance,
			    const char *msg)
{
	char *tfs_err_log = tfs_error_log(instance, NULL);

	if (tfs_err_log) {
		PyErr_Format(TFS_ERROR, "%s\ntfs_error: %s", msg, tfs_err_log);
		tfs_clear_error_log(instance);
		free(tfs_err_log);
	} else {
		PyErr_SetString(TFS_ERROR, msg);
	}
}

PyObject *PyTepRecord_time(PyTepRecord* self)
{
	unsigned long ts = self->ptrObj ? self->ptrObj->ts : 0;
	return PyLong_FromLongLong(ts);
}

PyObject *PyTepRecord_cpu(PyTepRecord* self)
{
	int cpu = self->ptrObj ? self->ptrObj->cpu : -1;
	return PyLong_FromLong(cpu);
}

PyObject *PyTepEvent_name(PyTepEvent* self)
{
	const char *name = self->ptrObj ? self->ptrObj->name : TC_NIL_MSG;
	return PyUnicode_FromString(name);
}

PyObject *PyTepEvent_id(PyTepEvent* self)
{
	int id = self->ptrObj ? self->ptrObj->id : -1;
	return PyLong_FromLong(id);
}

PyObject *PyTepEvent_field_names(PyTepEvent* self)
{
	struct tep_format_field *field, **fields;
	struct tep_event *event = self->ptrObj;
	int i = 0, nr_fields;
	PyObject *list;

	nr_fields= event->format.nr_fields + event->format.nr_common;
	list = PyList_New(nr_fields);

	/* Get all common fields. */
	fields = tep_event_common_fields(event);
	if (!fields) {
		PyErr_Format(TEP_ERROR,
			     "Failed to get common fields for event \'%s\'",
			     self->ptrObj->name);
		return NULL;
	}

	for (field = *fields; field; field = field->next)
		PyList_SET_ITEM(list, i++, PyUnicode_FromString(field->name));
	free(fields);

	/* Add all unique fields. */
	fields = tep_event_fields(event);
	if (!fields) {
		PyErr_Format(TEP_ERROR,
			     "Failed to get fields for event \'%s\'",
			     self->ptrObj->name);
		return NULL;
	}

	for (field = *fields; field; field = field->next)
		PyList_SET_ITEM(list, i++, PyUnicode_FromString(field->name));
	free(fields);

	return list;
}

static bool is_number(struct tep_format_field *field)
{
	int number_field_mask = TEP_FIELD_IS_SIGNED |
				TEP_FIELD_IS_LONG |
				TEP_FIELD_IS_FLAG;

	return !field->flags || field->flags & number_field_mask;
}

PyObject *PyTepEvent_parse_record_field(PyTepEvent* self, PyObject *args,
							  PyObject *kwargs)
{
	struct tep_format_field *field;
	int field_offset, field_size;
	const char *field_name;
	PyTepRecord *record;

	static char *kwlist[] = {"record", "field", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "Os",
					 kwlist,
					 &record,
					 &field_name)) {
		return NULL;
	}

	field = tep_find_field(self->ptrObj, field_name);
	if (!field)
		field = tep_find_common_field(self->ptrObj, field_name);

	if (!field) {
		PyErr_Format(TEP_ERROR,
			     "Failed to find field \'%s\' in event \'%s\'",
			     field_name, self->ptrObj->name);
		return NULL;
	}

	if (field->flags & TEP_FIELD_IS_DYNAMIC) {
		unsigned long long val;

		val = tep_read_number(self->ptrObj->tep,
				      record->ptrObj->data + field->offset,
				      field->size);
		field_offset = val & 0xffff;
		field_size = val >> 16;
	} else {
		field_offset = field->offset;
		field_size = field->size;
	}

	if (!field_size)
		return PyUnicode_FromString(TC_NIL_MSG);

	if (field->flags & TEP_FIELD_IS_STRING) {
		char *val_str = record->ptrObj->data + field_offset;
		return PyUnicode_FromString(val_str);
	} else if (is_number(field)) {
		unsigned long long val;

		tep_read_number_field(field, record->ptrObj->data, &val);
		return PyLong_FromLong(val);
	} else if (field->flags & TEP_FIELD_IS_POINTER) {
		void *val = record->ptrObj->data + field_offset;
		char ptr_string[11];

		sprintf(ptr_string, "%p", val);
		return PyUnicode_FromString(ptr_string);
	}

	PyErr_Format(TEP_ERROR,
		     "Unsupported field format \"%li\" (TODO: implement this)",
		     field->flags);
	return NULL;
}

int get_pid(struct tep_event *event, struct tep_record *record)
{
	const char *field_name = "common_pid";
	struct tep_format_field *field;
	unsigned long long val;

	field = tep_find_common_field(event, field_name);
	if (!field) {
		PyErr_Format(TEP_ERROR,
			     "Failed to find field \'s\' in event \'%s\'",
			     field_name, event->name);
		return -1;
	}

	tep_read_number_field(field, record->data, &val);

	return val;
}

PyObject *PyTepEvent_get_pid(PyTepEvent* self, PyObject *args,
					       PyObject *kwargs)
{
	static char *kwlist[] = {"record", NULL};
	PyTepRecord *record;
	int pid;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O",
					 kwlist,
					 &record)) {
		return NULL;
	}

	pid = get_pid(self->ptrObj, record->ptrObj);
	if (pid < 0)
		return NULL;

	return PyLong_FromLong(pid);
}

static const char *str_from_list(PyObject *py_list, int i)
{
	PyObject *item = PyList_GetItem(py_list, i);

	if (!PyUnicode_Check(item))
		return NULL;

	return PyUnicode_DATA(item);
}

static const char **get_arg_list(PyObject *py_list)
{
	const char **argv = NULL;
	int i, n;

	if (!PyList_CheckExact(py_list))
		goto fail;

	n = PyList_Size(py_list);
	argv = calloc(n + 1, sizeof(*argv));
	for (i = 0; i < n; ++i) {
		argv[i] = str_from_list(py_list, i);
		if (!argv[i])
			goto fail;
	}

	return argv;

 fail:
	free(argv);
	return NULL;
}

static struct tep_handle *get_tep(const char *dir, const char **sys_names)
{
	struct tep_handle *tep;

	tep = tracefs_local_events_system(dir, sys_names);
	if (!tep) {
		TfsError_fmt(NULL,
			     "Failed to get local 'tep' event from %s", dir);
		return NULL;
	}

	return tep;
}

PyObject *PyTep_init_local(PyTep *self, PyObject *args,
					PyObject *kwargs)
{
	static char *kwlist[] = {"dir", "systems", NULL};
	struct tep_handle *tep = NULL;
	PyObject *system_list = NULL;
	const char *dir_str;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|O",
					 kwlist,
					 &dir_str,
					 &system_list)) {
		return NULL;
	}

	if (system_list) {
		const char **sys_names = get_arg_list(system_list);

		if (!sys_names) {
			TfsError_setstr(NULL,
					"Inconsistent \"systems\" argument.");
			return NULL;
		}

		tep = get_tep(dir_str, sys_names);
		free(sys_names);
	} else {
		tep = get_tep(dir_str, NULL);
	}

	if (!tep)
		return NULL;

	tep_free(self->ptrObj);
	self->ptrObj = tep;

	Py_RETURN_NONE;
}

PyObject *PyTep_get_event(PyTep *self, PyObject *args,
				       PyObject *kwargs)
{
	static char *kwlist[] = {"system", "name", NULL};
	const char *system, *event_name;
	struct tep_event *event;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ss",
					 kwlist,
					 &system,
					 &event_name)) {
		return NULL;
	}

	event = tep_find_event_by_name(self->ptrObj, system, event_name);

	return PyTepEvent_New(event);
}

static struct trace_seq seq;

static bool init_print_seq(void)
{
	if (!seq.buffer)
		trace_seq_init(&seq);

	if (!seq.buffer) {
		PyErr_SetString(TFS_ERROR, "Unable to initialize 'trace_seq'.");
		return false;
	}

	trace_seq_reset(&seq);

	return true;
}

static inline void trim_new_line(char *val)
{
	if (val[strlen(val) - 1] == '\n')
		val[strlen(val) - 1] = '\0';
}

static char *get_comm_from_pid(int pid)
{
	char *comm_file, *comm = NULL;
	char buff[PATH_MAX] = {0};
	int fd, r;

	if (asprintf(&comm_file, "/proc/%i/comm", pid) <= 0) {
		MEM_ERROR;
		return NULL;
	}

	/*
	 * This file is not guaranteed to exist. Return NULL if the process
	 * is no longer active.
	 */
	fd = open(comm_file, O_RDONLY);
	free(comm_file);
	if (fd < 0)
		return NULL;

	r = read(fd, buff, PATH_MAX);
	close(fd);
	if (r <= 0)
		return NULL;

	trim_new_line(buff);
	comm = strdup(buff);
	if (!comm)
		MEM_ERROR;

	return comm;
}

static void print_comm_pid(struct tep_handle *tep,
			   struct trace_seq *seq,
			   struct tep_record *record,
			   struct tep_event *event)
{
	int pid = get_pid(event, record);
	if (!tep_is_pid_registered(tep, pid)) {
		char *comm = get_comm_from_pid(pid);
		if (comm) {
			tep_register_comm(tep, comm, pid);
			free(comm);
		}
	}

	tep_print_event(tep, seq, record, "%s-%i",
			TEP_PRINT_COMM,
			TEP_PRINT_PID);
}

static void print_name_info(struct tep_handle *tep,
			    struct trace_seq *seq,
			    struct tep_record *record,
			    struct tep_event *event)
{
	trace_seq_printf(seq, " %s: ", event->name);
	tep_print_event(tep, seq, record, "%s", TEP_PRINT_INFO);
}

static void print_event(struct tep_handle *tep,
			struct trace_seq *seq,
			struct tep_record *record,
			struct tep_event *event)
{
	tep_print_event(tep, seq, record, "%6.1000d ", TEP_PRINT_TIME);
	print_comm_pid(tep, seq, record, event);
	tep_print_event(tep, seq, record, " cpu=%i ", TEP_PRINT_CPU);
	print_name_info(tep, seq, record, event);
}

static bool print_init(PyObject *args, PyObject *kwargs,
		       struct tep_event **event,
		       struct tep_record **record)
{
	static char *kwlist[] = { "event", "record", NULL};
	PyObject *obj_rec, *obj_evt;
	PyTepRecord *py_record;
	PyTepEvent *py_event;

	if (!init_print_seq())
		return false;

	if(!PyArg_ParseTupleAndKeywords(args,
					kwargs,
					"OO",
					kwlist,
					&obj_evt,
					&obj_rec)) {
		return false;
	}

	if (PyTepEvent_Check(obj_evt) && PyTepRecord_Check(obj_rec)) {
		py_event = (PyTepEvent *)obj_evt;
		*event = py_event->ptrObj;
		py_record = (PyTepRecord *)obj_rec;
		*record = py_record->ptrObj;

		return true;
	}

	PyErr_SetString(TRACECRUNCHER_ERROR,
			"Inconsistent arguments.");

	return false;
}

PyObject *PyTep_event_record(PyTep *self, PyObject *args,
					  PyObject *kwargs)
{
	struct tep_record *record;
	struct tep_event *event;

	if (!print_init(args, kwargs, &event, &record))
		return NULL;

	print_event(self->ptrObj, &seq, record, event);

	return PyUnicode_FromString(seq.buffer);
}

PyObject *PyTep_info(PyTep *self, PyObject *args,
				  PyObject *kwargs)
{
	struct tep_record *record;
	struct tep_event *event;

	if (!print_init(args, kwargs, &event, &record))
		return NULL;

	print_name_info(self->ptrObj, &seq, record, event);

	return PyUnicode_FromString(seq.buffer);
}

PyObject *PyTep_process(PyTep *self, PyObject *args,
				     PyObject *kwargs)
{
	struct tep_record *record;
	struct tep_event *event;

	if (!print_init(args, kwargs, &event, &record))
		return NULL;

	print_comm_pid(self->ptrObj, &seq, record, event);

	return PyUnicode_FromString(seq.buffer);
}

static int kprobe_info_short(struct trace_seq *s,
			     struct tep_record *record,
			     struct tep_event *event,
			     void *context)
{
	/* Do not print the address of the probe (first field). */
	unsigned long long select_mask = ~0x1;

	tep_record_print_selected_fields(s, record, event, select_mask);

	return 0;
}

PyObject *PyTep_short_kprobe_print(PyTep *self, PyObject *args,
						PyObject *kwargs)
{
	static char *kwlist[] = {"system", "event", "id", NULL};
	const char *system, *event;
	int ret, id = -1;

	system = event = NO_ARG;

	if(!PyArg_ParseTupleAndKeywords(args,
					kwargs,
					"ss|i",
					kwlist,
					&system,
					&event,
					&id)) {
		return false;
	}

	ret = tep_register_event_handler(self->ptrObj, id, system, event,
					 kprobe_info_short, NULL);

	return PyLong_FromLong(ret);
}

static bool check_file(struct tracefs_instance *instance, const char *file)
{
	if (!tracefs_file_exists(instance, file)) {
		TfsError_fmt(instance, "File %s does not exist.", file);
		return false;
	}

	return true;
}

static bool check_dir(struct tracefs_instance *instance, const char *dir)
{
	if (!tracefs_dir_exists(instance, dir)) {
		TfsError_fmt(instance, "Directory %s does not exist.", dir);
		return false;
	}

	return true;
}

const char *top_instance_name = "top";
static const char *get_instance_name(struct tracefs_instance *instance)
{
	const char *name = tracefs_instance_get_name(instance);
	return name ? name : top_instance_name;
}

static int write_to_file(struct tracefs_instance *instance,
			 const char *file,
			 const char *val)
{
	int size;

	if (!check_file(instance, file))
		return -1;

	size = tracefs_instance_file_write(instance, file, val);
	if (size <= 0) {
		TfsError_fmt(instance,
			     "Can not write \'%s\' to file \'%s\' (inst: \'%s\').",
			     val, file, get_instance_name(instance));
		PyErr_Print();
	}

	return size;
}

static int append_to_file(struct tracefs_instance *instance,
			  const char *file,
			  const char *val)
{
	int size;

	if (!check_file(instance, file))
		return -1;

	size = tracefs_instance_file_append(instance, file, val);
	if (size <= 0) {
		TfsError_fmt(instance,
			     "Can not append \'%s\' to file \'%s\' (inst: \'%s\').",
			     val, file, get_instance_name(instance));
		PyErr_Print();
	}

	return size;
}

static int read_from_file(struct tracefs_instance *instance,
			  const char *file,
			  char **val)
{
	int size;

	if (!check_file(instance, file))
		return -1;

	*val = tracefs_instance_file_read(instance, file, &size);
	if (size < 0)
		TfsError_fmt(instance, "Can not read from file %s", file);

	return size;
}

static bool write_to_file_and_check(struct tracefs_instance *instance,
				    const char *file,
				    const char *val)
{
	char *read_val;
	int ret;

	if (write_to_file(instance, file, val) <= 0)
		return false;

	if (read_from_file(instance, file, &read_val) <= 0)
		return false;

	trim_new_line(read_val);
	ret = strcmp(read_val, val);
	free(read_val);

	return ret == 0 ? true : false;
}

static PyObject *tfs_list2py_list(char **list, bool sort)
{
	PyObject *py_list = PyList_New(0);
	int i;

	for (i = 0; list && list[i]; i++)
		PyList_Append(py_list, PyUnicode_FromString(list[i]));

	if (sort)
		PyList_Sort(py_list);

	tracefs_list_free(list);

	return py_list;
}

PyObject *PyTfsInstance_dir(PyTfsInstance *self)
{
	return PyUnicode_FromString(tracefs_instance_get_dir(self->ptrObj));
}

PyObject *PyTraceHist_add_value(PyTraceHist *self, PyObject *args,
						   PyObject *kwargs)
{
	static char *kwlist[] = {"value", NULL};
	const char *value;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s",
					 kwlist,
					 &value)) {
		return NULL;
	}

	if (tracefs_hist_add_value(self->ptrObj, value) < 0) {
		MEM_ERROR
		return NULL;
	}

	Py_RETURN_NONE;
}

const char *hist_noname = "unnamed";
static inline const char *get_hist_name(struct tracefs_hist *hist)
{
	const char *name = tracefs_hist_get_name(hist);
	return name ? name : hist_noname;
}

static bool add_sort_key(struct tracefs_hist *hist,
			 const char *sort_key)
{
	if (tracefs_hist_add_sort_key(hist, sort_key) < 0) {
		TfsError_fmt(NULL,
			     "Failed to add sort key \'%s\'to histogram \'%s\'.",
			     sort_key, get_hist_name(hist));
		return false;
	}

	return true;
}

PyObject *PyTraceHist_sort_keys(PyTraceHist *self, PyObject *args,
						   PyObject *kwargs)
{
	static char *kwlist[] = {"keys", NULL};
	PyObject *py_obj;
	const char *key;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O",
					 kwlist,
					 &py_obj)) {
		return NULL;
	}

	if (PyUnicode_Check(py_obj)) {
		if (!add_sort_key(self->ptrObj, PyUnicode_DATA(py_obj)))
			return NULL;
	} else if (PyList_CheckExact(py_obj)) {
		int i, n = PyList_Size(py_obj);

		for (i = 0; i < n; ++i) {
			key = str_from_list(py_obj, i);
			if (!key ||
			    !add_sort_key(self->ptrObj, key)) {
				PyErr_SetString(TRACECRUNCHER_ERROR,
						"Inconsistent \"keys\" argument.");
				return NULL;
			}
		}
	}

	Py_RETURN_NONE;
}

static int sort_direction(PyObject *py_dir)
{
	int dir = -1;

	if (PyLong_CheckExact(py_dir)) {
		dir = PyLong_AsLong(py_dir);
	} else if (PyUnicode_Check(py_dir)) {
		const char *dir_str =  PyUnicode_DATA(py_dir);

		if (lax_cmp(dir_str, "descending") ||
		    lax_cmp(dir_str, "desc") ||
		    lax_cmp(dir_str, "d"))
			dir = 1;
		else if (lax_cmp(dir_str, "ascending") ||
			 lax_cmp(dir_str, "asc") ||
			 lax_cmp(dir_str, "a"))
			dir = 0;
	}

	return dir;
}

PyObject *PyTraceHist_sort_key_direction(PyTraceHist *self, PyObject *args,
							    PyObject *kwargs)
{
	static char *kwlist[] = {"sort_key", "direction", NULL};
	const char *sort_key;
	PyObject *py_dir;
	int dir;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "sO",
					 kwlist,
					 &sort_key,
					 &py_dir)) {
		return NULL;
	}

	dir = sort_direction(py_dir);

	if (dir < 0 ||
	    tracefs_hist_sort_key_direction(self->ptrObj, sort_key, dir) < 0) {
		TfsError_fmt(NULL,
			     "Failed to add sort direction to histogram \'%s\'.",
			     get_hist_name(self->ptrObj));
		return NULL;
	}

	Py_RETURN_NONE;
}

static bool get_optional_instance(PyObject *py_obj,
				  struct tracefs_instance **instance)
{
	PyTfsInstance *py_inst;

	if (!py_obj) {
		*instance = NULL;
		return true;
	}

	if (!PyTfsInstance_Check(py_obj)) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Passing argument \'instance\' with incompatible type.");
		return false;
	}

	py_inst = (PyTfsInstance *)py_obj;
	*instance = py_inst->ptrObj;

	return true;
}

bool get_instance_from_arg(PyObject *args, PyObject *kwargs,
			   struct tracefs_instance **instance)
{
	static char *kwlist[] = {"instance", NULL};
	PyObject *py_inst = NULL;
	*instance = NULL;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|O",
					 kwlist,
					 &py_inst)) {
		return false;
	}

	if (!get_optional_instance(py_inst, instance))
		return NULL;

	return true;
}

static bool hist_cmd(PyTraceHist *self, PyObject *args, PyObject *kwargs,
		     enum tracefs_hist_command cmd,
		     const char *err_msg)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	if (tracefs_hist_command(instance, self->ptrObj, cmd) < 0) {
		char *buff;

		if (asprintf(&buff, "%s %s",
			     err_msg, get_hist_name(self->ptrObj)) <= 0) {
			MEM_ERROR;
			return false;
		}

		TfsError_setstr(instance, buff);
		free(buff);

		return false;
	}

	return true;
}

PyObject *PyTraceHist_start(PyTraceHist *self, PyObject *args,
					       PyObject *kwargs)
{
	if (!hist_cmd(self, args, kwargs,
		      TRACEFS_HIST_CMD_START,
		      "Failed to start filling the histogram"))
		return false;

	Py_RETURN_NONE;
}

PyObject *PyTraceHist_stop(PyTraceHist *self, PyObject *args,
					      PyObject *kwargs)
{
	if (!hist_cmd(self, args, kwargs,
		      TRACEFS_HIST_CMD_PAUSE,
		      "Failed to stop filling the histogram"))
		return false;

	Py_RETURN_NONE;
}

PyObject *PyTraceHist_resume(PyTraceHist *self, PyObject *args,
						PyObject *kwargs)
{
	if (!hist_cmd(self, args, kwargs,
		      TRACEFS_HIST_CMD_CONT,
		      "Failed to resume filling the histogram"))
		return false;

	Py_RETURN_NONE;
}

PyObject *PyTraceHist_clear(PyTraceHist *self, PyObject *args,
					       PyObject *kwargs)
{
	if (!hist_cmd(self, args, kwargs,
		      TRACEFS_HIST_CMD_CLEAR,
		      "Failed to clear the histogram"))
		return false;

	Py_RETURN_NONE;
}

static char *hist_read(PyTraceHist *self, PyObject *args,
					  PyObject *kwargs)
{
	struct tracefs_instance *instance;
	const char *hist_file = "hist";
	char *data;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	data = tracefs_event_file_read(instance,
				       tracefs_hist_get_system(self->ptrObj),
				       tracefs_hist_get_event(self->ptrObj),
				       hist_file,
				       NULL);
	if (!data) {
		TfsError_fmt(instance,
			     "Failed read data from histogram \'%s\'.",
			     get_hist_name(self->ptrObj));
	}

	return data;
}

PyObject *PyTraceHist_read(PyTraceHist *self, PyObject *args,
					      PyObject *kwargs)
{
	char *data = hist_read(self, args, kwargs);
	if (!data)
		return NULL;

	PyObject *ret = PyUnicode_FromString(data);

	free(data);
	return ret;
}

PyObject *PyTraceHist_close(PyTraceHist *self, PyObject *args,
					       PyObject *kwargs)
{
	if (!hist_cmd(self, args, kwargs,
		      TRACEFS_HIST_CMD_DESTROY,
		      "Failed to close the histogram"))
		return false;

	Py_RETURN_NONE;
}

typedef int (*add_field_ptr)(struct tracefs_synth *,
			     const char *,
			     const char *);

PyObject *synth_add_fields(PySynthEvent *self, PyObject *args,
					       PyObject *kwargs,
					       bool to_start)
{
	static char *kwlist[] = {"fields", "names", NULL};
	PyObject *py_fields, *py_names = NULL;
	PyObject *item_field, *item_name;
	add_field_ptr add_field_func;
	const char *field, *name;
	int ret, n, i;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|O",
					 kwlist,
					 &py_fields,
					 &py_names)) {
		return NULL;
	}

	add_field_func = to_start ? tracefs_synth_add_start_field:
				    tracefs_synth_add_end_field;

	n = PyList_Size(py_fields);
	for (i = 0; i < n; ++i) {
		item_field = PyList_GetItem(py_fields, i);
		field = PyUnicode_DATA(item_field);

		name = NULL;
		if (py_names) {
			item_name = PyList_GetItem(py_names, i);
			if (item_name && item_name != Py_None)
				name = PyUnicode_DATA(item_name);
		}

		ret = add_field_func(self->ptrObj, field, name);
		if (ret < 0) {
			TfsError_fmt(NULL,
				     "Failed to add %s field '%s' to synth. event %s",
				     to_start ? "start" : "end", field,
				     tracefs_synth_get_name(self->ptrObj));
			return NULL;
		}
	}

	Py_RETURN_NONE;
}

PyObject *PySynthEvent_add_start_fields(PySynthEvent *self, PyObject *args,
							    PyObject *kwargs)
{
	return synth_add_fields(self, args, kwargs, true);
}

PyObject *PySynthEvent_add_end_fields(PySynthEvent *self, PyObject *args,
							  PyObject *kwargs)
{
	return synth_add_fields(self, args, kwargs, false);
}

static inline void add_synth_field_err(const char *field, const char *event)
{
	TfsError_fmt(NULL, "Failed to add field '%s' to synth. event %s",
		     field, event);
}

static inline PyObject *
add_synth_field(PySynthEvent *self, PyObject *args, PyObject *kwargs,
		enum tracefs_synth_calc calc)
{
	static char *kwlist[] = {"name", "start_field", "end_field", NULL};
	const char *start_field, *end_field, *name;
	int ret;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "sss",
					 kwlist,
					 &name,
					 &start_field,
					 &end_field)) {
		return NULL;
	}

	ret = tracefs_synth_add_compare_field(self->ptrObj,
					      start_field, end_field, calc,
					      name);
	if (ret < 0) {
		add_synth_field_err(name, tracefs_synth_get_name(self->ptrObj));
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject *PySynthEvent_add_delta_start(PySynthEvent *self, PyObject *args,
							   PyObject *kwargs)
{
	return add_synth_field(self, args, kwargs, TRACEFS_SYNTH_DELTA_START);
}

PyObject *PySynthEvent_add_delta_end(PySynthEvent *self, PyObject *args,
							 PyObject *kwargs)
{
	return add_synth_field(self, args, kwargs, TRACEFS_SYNTH_DELTA_END);
}

PyObject *PySynthEvent_add_sum(PySynthEvent *self, PyObject *args,
						   PyObject *kwargs)
{
	return add_synth_field(self, args, kwargs, TRACEFS_SYNTH_ADD);
}

PyObject *PySynthEvent_add_delta_T(PySynthEvent *self, PyObject *args,
						       PyObject *kwargs)
{
	static char *kwlist[] = {"name", "hd", NULL};
	const char *name = "delta_T";
	const char* time_rez;
	int ret, hd = 0;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|sp",
					 kwlist,
					 &name,
					 &hd)) {
		return NULL;
	}

	time_rez = hd ? TRACEFS_TIMESTAMP : TRACEFS_TIMESTAMP_USECS;
	ret = tracefs_synth_add_compare_field(self->ptrObj, time_rez, time_rez,
					      TRACEFS_SYNTH_DELTA_END, name);
	if (ret < 0) {
		add_synth_field_err(name, tracefs_synth_get_name(self->ptrObj));
		return NULL;
	}

	Py_RETURN_NONE;
}

static void set_destroy_flag(PyObject *py_obj, bool val)
{
	PyFtrace_Object_HEAD *obj_head = (PyFtrace_Object_HEAD *)py_obj;
	obj_head->destroy = val;
}

PyObject *PySynthEvent_register(PySynthEvent *self)
{
	if (tracefs_synth_create(self->ptrObj) < 0) {
		TfsError_fmt(NULL, "Failed to register synth. event %s",
			     tracefs_synth_get_name(self->ptrObj));
		return NULL;
	}

	/*
	 * Here the synthetic event gets added to the system.
	 * Hence we need to 'destroy' this event at exit.
	 */
	set_destroy_flag((PyObject *)self, true);
	Py_RETURN_NONE;
}

PyObject *PySynthEvent_unregister(PySynthEvent *self)
{
	if (tracefs_synth_destroy(self->ptrObj) < 0) {
		TfsError_fmt(NULL, "Failed to unregister synth. event %s",
			     tracefs_synth_get_name(self->ptrObj));
		return NULL;
	}

	/*
	 * Here the synthetic event gets removed from the system.
	 * Hence we no loger need to 'destroy' this event at exit.
	 */
	set_destroy_flag((PyObject *)self, false);
	Py_RETURN_NONE;
}

PyObject *PyFtrace_dir(PyObject *self)
{
	return PyUnicode_FromString(tracefs_tracing_dir());
}

static PyObject *set_destroy(PyObject *args, PyObject *kwargs, bool destroy)
{
	static char *kwlist[] = {"object", NULL};
	PyObject *py_obj;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O",
					 kwlist,
					 &py_obj)) {
		return NULL;
	}

	set_destroy_flag(py_obj, destroy);

	Py_RETURN_NONE;
}

PyObject *PyFtrace_detach(PyObject *self, PyObject *args, PyObject *kwargs)
{
	return set_destroy(args, kwargs, false);
}

PyObject *PyFtrace_attach(PyObject *self, PyObject *args, PyObject *kwargs)
{
	return set_destroy(args, kwargs, true);
}

static bool get_destroy_flag(PyObject *py_obj)
{
	PyFtrace_Object_HEAD *obj_head = (PyFtrace_Object_HEAD *)py_obj;
	return obj_head->destroy;
}

PyObject *PyFtrace_is_attached(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"object", NULL};
	PyObject *py_obj;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O",
					 kwlist,
					 &py_obj)) {
		return NULL;
	}

	return get_destroy_flag(py_obj) ? Py_True : Py_False;
}

static char aname_pool[] =
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

#define ANAME_LEN	16

char auto_name[ANAME_LEN];

static const char *autoname()
{
	int i, n, pool_size = sizeof(aname_pool);
	struct timeval now;

	gettimeofday(&now, NULL);
	srand(now.tv_usec);

	for (i = 0; i < ANAME_LEN - 1; ++i) {
		n = rand() % (pool_size - 1);
		auto_name[i] = aname_pool[n];
	}
	auto_name[i] = 0;

	return auto_name;
}

static bool tracing_OFF(struct tracefs_instance *instance);

PyObject *PyFtrace_create_instance(PyObject *self, PyObject *args,
						   PyObject *kwargs)
{
	struct tracefs_instance *instance;
	const char *name = NO_ARG;
	int tracing_on = true;

	static char *kwlist[] = {"name", "tracing_on", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|sp",
					 kwlist,
					 &name,
					 &tracing_on)) {
		return NULL;
	}

	if (!is_set(name))
		name = autoname();

	instance = tracefs_instance_create(name);
	if (!instance ||
	    !tracefs_instance_exists(name) ||
	    !tracefs_instance_is_new(instance)) {
		TfsError_fmt(instance,
			     "Failed to create new trace instance \'%s\'.",
			     name);
		return NULL;
	}

	if (!tracing_on)
		tracing_OFF(instance);

	return PyTfsInstance_New(instance);
}

PyObject *PyFtrace_find_instance(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *py_inst;
	const char *name;

	static char *kwlist[] = {"name", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s",
					 kwlist,
					 &name)) {
		return NULL;
	}

	instance = tracefs_instance_alloc(NULL, name);
	if (!instance) {
		TfsError_fmt(instance,
			     "Failed to find trace instance \'%s\'.",
			     name);
		return NULL;
	}

	py_inst = PyTfsInstance_New(instance);
	set_destroy_flag(py_inst, false);

	return py_inst;
}

PyObject *PyFtrace_available_tracers(PyObject *self, PyObject *args,
						     PyObject *kwargs)
{
	struct tracefs_instance *instance;
	char **list;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	list = tracefs_tracers(tracefs_instance_get_dir(instance));
	if (!list)
		return NULL;

	return tfs_list2py_list(list, false);
}

PyObject *PyFtrace_set_current_tracer(PyObject *self, PyObject *args,
						      PyObject *kwargs)
{
	static char *kwlist[] = {"tracer", "instance", NULL};
	const char *file = "current_tracer", *tracer;
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;

	tracer = NO_ARG;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|sO",
					 kwlist,
					 &tracer,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (is_set(tracer) &&
	    strcmp(tracer, "nop") != 0) {
		char **all_tracers =
			tracefs_tracers(tracefs_instance_get_dir(instance));
		int i;

		for (i = 0; all_tracers && all_tracers[i]; i++) {
			if (!strcmp(all_tracers[i], tracer))
				break;
		}

		if (!all_tracers || !all_tracers[i]) {
			TfsError_fmt(instance,
				     "Tracer \'%s\' is not available.",
				     tracer);
			return NULL;
		}
	} else if (!is_set(tracer)) {
		tracer = "nop";
	}

	if (!write_to_file_and_check(instance, file, tracer)) {
		TfsError_fmt(instance, "Failed to enable tracer \'%s\'",
			     tracer);
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject *PyFtrace_get_current_tracer(PyObject *self, PyObject *args,
						      PyObject *kwargs)
{
	const char *file = "current_tracer";
	struct tracefs_instance *instance;
	PyObject *ret;
	char *tracer;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	if (read_from_file(instance, file, &tracer) <= 0)
		return NULL;

	trim_new_line(tracer);
	ret = PyUnicode_FromString(tracer);
	free(tracer);

	return ret;
}

PyObject *PyFtrace_available_event_systems(PyObject *self, PyObject *args,
							   PyObject *kwargs)
{
	static char *kwlist[] = {"instance", "sort", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	int sort = false;
	char **list;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|Op",
					 kwlist,
					 &py_inst,
					 &sort)) {
		return false;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	list = tracefs_event_systems(tracefs_instance_get_dir(instance));
	if (!list)
		return NULL;

	return tfs_list2py_list(list, sort);
}

PyObject *PyFtrace_available_system_events(PyObject *self, PyObject *args,
							   PyObject *kwargs)
{
	static char *kwlist[] = {"system", "instance", "sort", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	const char *system;
	int sort = false;
	char **list;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|Op",
					 kwlist,
					 &system,
					 &py_inst,
					 &sort)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	list = tracefs_system_events(tracefs_instance_get_dir(instance),
				     system);

	if (!list)
		return NULL;

	return tfs_list2py_list(list, sort);
}

bool get_event_enable_file(struct tracefs_instance *instance,
			   const char *system, const char *event,
			   char **path)
{
	char *buff = calloc(PATH_MAX, 1);
	const char *instance_name;

	 if (!buff) {
		MEM_ERROR
		return false;
	}

	if ((is_all(system) && is_all(event)) ||
	    (is_all(system) && is_no_arg(event)) ||
	    (is_no_arg(system) && is_all(event))) {
		strcpy(buff, "events/enable");

		*path = buff;
	} else if (is_set(system)) {
		strcpy(buff, "events/");
		strcat(buff, system);
		if (!check_dir(instance, buff))
			goto fail;

		if (is_set(event)) {
			strcat(buff, "/");
			strcat(buff, event);
			if (!check_dir(instance, buff))
				goto fail;

			strcat(buff, "/enable");
		} else {
			strcat(buff, "/enable");
		}

		*path = buff;
	} else {
		goto fail;
	}

	return true;

 fail:
	instance_name =
		instance ? tracefs_instance_get_name(instance) : "top";
	TfsError_fmt(instance,
		     "Failed to locate event:\n Instance: %s  System: %s  Event: %s",
		     instance_name, system, event);
	free(buff);
	*path = NULL;
	return false;
}

static bool event_enable_disable(struct tracefs_instance *instance,
				 const char *system, const char *event,
				 bool enable)
{
	int ret;

	if (system && !is_set(system))
		system = NULL;

	if (event && !is_set(event))
		event = NULL;

	if (enable)
		ret = tracefs_event_enable(instance, system, event);
	else
		ret = tracefs_event_disable(instance, system, event);

	if (ret != 0) {
		TfsError_fmt(instance,
			     "Failed to enable/disable event:\n System: %s  Event: %s",
			     system ? system : "NULL",
			     event ? event : "NULL");

		return false;
	}

	return true;
}

static bool set_enable_event(PyObject *self,
			     PyObject *args, PyObject *kwargs,
			     bool enable)
{
	static char *kwlist[] = {"instance", "system", "event", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	const char *system, *event;

	system = event = NO_ARG;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|Oss",
					 kwlist,
					 &py_inst,
					 &system,
					 &event)) {
		return false;
	}

	if (!get_optional_instance(py_inst, &instance))
		return false;

	return event_enable_disable(instance, system, event, enable);
}

#define ON	"1"
#define OFF	"0"

PyObject *PyFtrace_enable_event(PyObject *self, PyObject *args,
						PyObject *kwargs)
{
	if (!set_enable_event(self, args, kwargs, true))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyFtrace_disable_event(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	if (!set_enable_event(self, args, kwargs, false))
		return NULL;

	Py_RETURN_NONE;
}

static bool set_enable_events(PyObject *self, PyObject *args, PyObject *kwargs,
			      bool enable)
{
	static char *kwlist[] = {"events", "instance", NULL};
	PyObject *event_dict = NULL, *py_inst = NULL;
	struct tracefs_instance *instance;
	PyObject *py_system, *py_events;
	const char *system, *event;
	Py_ssize_t pos = 0;
	int i, n;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|O",
					 kwlist,
					 &event_dict,
					 &py_inst)) {
		return false;
	}

	if (!get_optional_instance(py_inst, &instance))
		return false;

	if (!PyDict_CheckExact(event_dict))
		goto fail_with_err;

	while (PyDict_Next(event_dict, &pos, &py_system, &py_events)) {
		if (!PyUnicode_Check(py_system) ||
		    !PyList_CheckExact(py_events))
			goto fail_with_err;

		system = PyUnicode_DATA(py_system);
		n = PyList_Size(py_events);

		if (n == 0 || (n == 1 && is_all(str_from_list(py_events, 0)))) {
			if (!event_enable_disable(instance, system, NULL,
						  enable))
				return false;
			continue;
		}

		for (i = 0; i < n; ++i) {
			event = str_from_list(py_events, i);
			if (!event)
				goto fail_with_err;

			if (!event_enable_disable(instance, system, event,
						  enable))
				return false;
		}
	}

	return true;

 fail_with_err:
	TfsError_setstr(instance, "Inconsistent \"events\" argument.");
	return false;
}

PyObject *PyFtrace_enable_events(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	if (!set_enable_events(self, args, kwargs, true))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyFtrace_disable_events(PyObject *self, PyObject *args,
						  PyObject *kwargs)
{
	if (!set_enable_events(self, args, kwargs, false))
		return NULL;

	Py_RETURN_NONE;
}

static PyObject *event_is_enabled(struct tracefs_instance *instance,
				  const char *system, const char *event)
{
	char *file, *val;
	PyObject *ret;

	if (!get_event_enable_file(instance, system, event, &file))
		return NULL;

	if (read_from_file(instance, file, &val) <= 0)
		return NULL;

	trim_new_line(val);
	ret = PyUnicode_FromString(val);

	free(file);
	free(val);

	return ret;
}

PyObject *PyFtrace_event_is_enabled(PyObject *self, PyObject *args,
						    PyObject *kwargs)
{
	static char *kwlist[] = {"instance", "system", "event", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	const char *system, *event;

	system = event = NO_ARG;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|Oss",
					 kwlist,
					 &py_inst,
					 &system,
					 &event)) {
		return false;
	}

	if (!get_optional_instance(py_inst, &instance))
		return false;

	return event_is_enabled(instance, system, event);
}

PyObject *PyFtrace_set_event_filter(PyObject *self, PyObject *args,
						    PyObject *kwargs)
{
	const char *system, *event, *filter;
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	char path[PATH_MAX];

	static char *kwlist[] = {"system", "event", "filter", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "sss|O",
					 kwlist,
					 &system,
					 &event,
					 &filter,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	sprintf(path, "events/%s/%s/filter", system, event);
	if (!write_to_file_and_check(instance, path, filter)) {
		TfsError_setstr(instance, "Failed to set event filter");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject *PyFtrace_clear_event_filter(PyObject *self, PyObject *args,
						      PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	const char *system, *event;
	char path[PATH_MAX];

	static char *kwlist[] = {"system", "event", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ss|O",
					 kwlist,
					 &system,
					 &event,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	sprintf(path, "events/%s/%s/filter", system, event);
	if (!write_to_file(instance, path, OFF)) {
		TfsError_setstr(instance, "Failed to clear event filter");
		return NULL;
	}

	Py_RETURN_NONE;
}

static bool tracing_ON(struct tracefs_instance *instance)
{
	int ret = tracefs_trace_on(instance);

	if (ret < 0 ||
	    tracefs_trace_is_on(instance) != 1) {
		const char *instance_name =
			instance ? tracefs_instance_get_name(instance) : "top";

		TfsError_fmt(instance,
			     "Failed to start tracing (Instance: %s)",
			     instance_name);
		return false;
	}

	return true;
}

PyObject *PyFtrace_tracing_ON(PyObject *self, PyObject *args,
					      PyObject *kwargs)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	if (!tracing_ON(instance))
		return NULL;

	Py_RETURN_NONE;
}

static bool tracing_OFF(struct tracefs_instance *instance)
{
	int ret = tracefs_trace_off(instance);

	if (ret < 0 ||
	    tracefs_trace_is_on(instance) != 0) {
		const char *instance_name =
			instance ? tracefs_instance_get_name(instance) : "top";

		TfsError_fmt(instance,
			     "Failed to stop tracing (Instance: %s)",
			     instance_name);
		return false;
	}

	return true;
}

PyObject *PyFtrace_tracing_OFF(PyObject *self, PyObject *args,
					       PyObject *kwargs)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	if (!tracing_OFF(instance))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyFtrace_is_tracing_ON(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	struct tracefs_instance *instance;
	int ret;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	ret = tracefs_trace_is_on(instance);
	if (ret < 0) {
		const char *instance_name =
			instance ? tracefs_instance_get_name(instance) : "top";

		TfsError_fmt(instance,
			     "Failed to check if tracing is ON (Instance: %s)",
			     instance_name);
		return NULL;
	}

	if (ret == 0)
		Py_RETURN_FALSE;

	Py_RETURN_TRUE;
}

static bool pid2file(struct tracefs_instance *instance,
		     const char *file,
		     int pid,
		     bool append)
{
	char pid_str[100];

	if (sprintf(pid_str, "%d", pid) <= 0)
		return false;

	if (append) {
		if (!append_to_file(instance, file, pid_str))
		        return false;
	} else {
		if (!write_to_file_and_check(instance, file, pid_str))
		        return false;
	}

	return true;
}

static bool set_pid(struct tracefs_instance *instance,
		    const char *file, PyObject *pid_val)
{
	PyObject *item;
	int n, i, pid;

	if (PyList_CheckExact(pid_val)) {
		n = PyList_Size(pid_val);
		for (i = 0; i < n; ++i) {
			item = PyList_GetItem(pid_val, i);
			if (!PyLong_CheckExact(item))
				goto fail;

			pid = PyLong_AsLong(item);
			if (!pid2file(instance, file, pid, true))
				goto fail;
		}
	} else if (PyLong_CheckExact(pid_val)) {
		pid = PyLong_AsLong(pid_val);
		if (!pid2file(instance, file, pid, true))
			goto fail;
	} else {
		goto fail;
	}

	return true;

 fail:
	TfsError_fmt(instance, "Failed to set PIDs for \"%s\"",
		     file);
	return false;
}

PyObject *PyFtrace_set_event_pid(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	PyObject *pid_val;

	static char *kwlist[] = {"pid", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|O",
					 kwlist,
					 &pid_val,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (!set_pid(instance, "set_event_pid", pid_val))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyFtrace_set_ftrace_pid(PyObject *self, PyObject *args,
						  PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	PyObject *pid_val;

	static char *kwlist[] = {"pid", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|O",
					 kwlist,
					 &pid_val,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (!set_pid(instance, "set_ftrace_pid", pid_val))
		return NULL;

	Py_RETURN_NONE;
}

static bool set_opt(struct tracefs_instance *instance,
		    const char *opt, const char *val)
{
	char file[PATH_MAX];

	if (sprintf(file, "options/%s", opt) <= 0 ||
	    !write_to_file_and_check(instance, file, val)) {
		TfsError_fmt(instance, "Failed to set option \"%s\"", opt);
		return false;
	}

	return true;
}

static PyObject *set_option_py_args(PyObject *args, PyObject *kwargs,
				   const char *val)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	const char *opt;

	static char *kwlist[] = {"option", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|O",
					 kwlist,
					 &opt,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (!set_opt(instance, opt, val))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyFtrace_enable_option(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	return set_option_py_args(args, kwargs, ON);
}

PyObject *PyFtrace_disable_option(PyObject *self, PyObject *args,
						  PyObject *kwargs)
{
	return set_option_py_args(args, kwargs, OFF);
}

PyObject *PyFtrace_option_is_set(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	enum tracefs_option_id opt_id;
	const char *opt;

	static char *kwlist[] = {"option", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|O",
					 kwlist,
					 &opt,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	opt_id = tracefs_option_id(opt);
	if (tracefs_option_is_enabled(instance, opt_id))
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

static PyObject *get_option_list(struct tracefs_instance *instance,
				 bool enabled)
{
	const struct tracefs_options_mask *mask;
	PyObject *list = PyList_New(0);
	int i;

	mask = enabled ? tracefs_options_get_enabled(instance) :
			 tracefs_options_get_supported(instance);

	for (i = 0; i < TRACEFS_OPTION_MAX; ++i)
		if (tracefs_option_mask_is_set(mask, i)) {
			const char *opt = tracefs_option_name(i);
			PyList_Append(list, PyUnicode_FromString(opt));
		}

	return list;
}

PyObject *PyFtrace_enabled_options(PyObject *self, PyObject *args,
						   PyObject *kwargs)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	return get_option_list(instance, true);
}

PyObject *PyFtrace_supported_options(PyObject *self, PyObject *args,
						     PyObject *kwargs)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	return get_option_list(instance, false);
}

#define TC_SYS	"tcrunch"

PyObject *PyFtrace_tc_event_system(PyObject *self)
{
	return PyUnicode_FromString(TC_SYS);
}

static PyObject *dynevent_info2py(char *buff, int type)
{
	PyObject *ret;

	if (type == TRACEFS_DYNEVENT_UNKNOWN) {
		PyErr_SetString(TFS_ERROR, "Failed to get dynevent info.");
		return NULL;
	}

	ret = PyUnicode_FromString(buff);
	free(buff);

	return ret;
}

PyObject *PyDynevent_system(PyDynevent *self)
{
	char *buff;
	int type;

	type = tracefs_dynevent_info(self->ptrObj, &buff, NULL, NULL, NULL, NULL);
	return dynevent_info2py(buff, type);
}

PyObject *PyDynevent_event(PyDynevent *self)
{
	char *buff;
	int type;

	type = tracefs_dynevent_info(self->ptrObj, NULL, &buff, NULL, NULL, NULL);
	return dynevent_info2py(buff, type);
}

PyObject *PyDynevent_address(PyDynevent *self)
{
	char *buff;
	int type;

	type = tracefs_dynevent_info(self->ptrObj, NULL, NULL, NULL, &buff, NULL);
	return dynevent_info2py(buff, type);
}

PyObject *PyDynevent_probe(PyDynevent *self)
{
	char *buff;
	int type;

	type = tracefs_dynevent_info(self->ptrObj, NULL, NULL, NULL, NULL, &buff);
	return dynevent_info2py(buff, type);
}

PyObject *PyFtrace_register_kprobe(PyObject *self, PyObject *args,
						   PyObject *kwargs)
{
	static char *kwlist[] = {"event", "function", "probe", NULL};
	const char *event, *function, *probe;
	struct tracefs_dynevent *kprobe;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "sss",
					 kwlist,
					 &event,
					 &function,
					 &probe)) {
		return NULL;
	}

	kprobe = tracefs_kprobe_alloc(TC_SYS, event, function, probe);
	if (!kprobe) {
		MEM_ERROR;
		return NULL;
	}

	if (tracefs_dynevent_create(kprobe) < 0) {
		TfsError_fmt(NULL, "Failed to create kprobe '%s'", event);
		tracefs_dynevent_free(kprobe);
		return NULL;
	}

	return PyDynevent_New(kprobe);
}

PyObject *PyFtrace_register_kretprobe(PyObject *self, PyObject *args,
						      PyObject *kwargs)
{
	static char *kwlist[] = {"event", "function", "probe", NULL};
	const char *event, *function, *probe = "$retval";
	struct tracefs_dynevent *kprobe;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ss|s",
					 kwlist,
					 &event,
					 &function,
					 &probe)) {
		return NULL;
	}

	kprobe = tracefs_kretprobe_alloc(TC_SYS, event, function, probe, 0);
	if (!kprobe) {
		MEM_ERROR;
		return NULL;
	}

	if (tracefs_dynevent_create(kprobe) < 0) {
		TfsError_fmt(NULL, "Failed to create kretprobe '%s'", event);
		tracefs_dynevent_free(kprobe);
		return NULL;
	}

	return PyDynevent_New(kprobe);
}

PyObject *PyDynevent_set_filter(PyDynevent *self, PyObject *args,
						  PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	struct tep_handle * tep;
	struct tep_event *event;
	const char *filter;

	static char *kwlist[] = {"filter", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|O",
					 kwlist,
					 &filter,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	tep = get_tep(NULL, NULL);
	if (!tep)
		return NULL;

	event = tracefs_dynevent_get_event(tep, self->ptrObj);
	if (!event) {
		TfsError_setstr(NULL, "Failed to get event.");
		return NULL;
	}

	if (tracefs_event_filter_apply(instance, event, filter) < 0) {
		TfsError_fmt(NULL, "Failed to apply filter '%s' to event '%s'.",
			     filter, event->name);
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject *PyDynevent_get_filter(PyDynevent *self, PyObject *args,
						  PyObject *kwargs)
{
	char *evt_name, *evt_system, *filter = NULL;
	struct tracefs_instance *instance;
	PyObject *ret = NULL;
	char path[PATH_MAX];
	int type;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	type = tracefs_dynevent_info(self->ptrObj, &evt_system, &evt_name,
				     NULL, NULL, NULL);
	if (type == TRACEFS_DYNEVENT_UNKNOWN) {
		PyErr_SetString(TFS_ERROR, "Failed to get dynevent info.");
		return NULL;
	}

	sprintf(path, "events/%s/%s/filter", evt_system, evt_name);
	if (read_from_file(instance, path, &filter) <= 0)
		goto free;

	trim_new_line(filter);
	ret = PyUnicode_FromString(filter);
 free:
	free(evt_system);
	free(evt_name);
	free(filter);

	return ret;
}

PyObject *PyDynevent_clear_filter(PyDynevent *self, PyObject *args,
						    PyObject *kwargs)
{
	struct tracefs_instance *instance;
	struct tep_handle * tep;
	struct tep_event *event;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	tep = get_tep(NULL, NULL);
	if (!tep)
		return NULL;

	event = tracefs_dynevent_get_event(tep, self->ptrObj);
	if (!event) {
		TfsError_setstr(NULL, "Failed to get event.");
		return NULL;
	}

	if (tracefs_event_filter_clear(instance, event) < 0) {
		TfsError_fmt(NULL, "Failed to clear filter for event '%s'.",
			     event->name);
		return NULL;
	}

	Py_RETURN_NONE;
}

static bool enable_dynevent(PyDynevent *self, PyObject *args, PyObject *kwargs,
			  bool enable)
{
	struct tracefs_instance *instance;
	char * evt_name;
	int type;
	bool ret;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return false;

	type = tracefs_dynevent_info(self->ptrObj, NULL, &evt_name, NULL, NULL, NULL);
	if (type == TRACEFS_DYNEVENT_UNKNOWN) {
		PyErr_SetString(TFS_ERROR, "Failed to get dynevent info.");
		return NULL;
	}

	ret = event_enable_disable(instance, TC_SYS, evt_name, enable);
	free(evt_name);

	return ret;
}

PyObject *PyDynevent_enable(PyDynevent *self, PyObject *args,
					      PyObject *kwargs)
{
	if (!enable_dynevent(self, args, kwargs, true))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyDynevent_disable(PyDynevent *self, PyObject *args,
					       PyObject *kwargs)
{
	if (!enable_dynevent(self, args, kwargs, false))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyDynevent_is_enabled(PyDynevent *self, PyObject *args,
					      PyObject *kwargs)
{
	struct tracefs_instance *instance;
	char * evt_name;
	PyObject *ret;
	int type;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	type = tracefs_dynevent_info(self->ptrObj, NULL, &evt_name, NULL, NULL, NULL);
	if (type == TRACEFS_DYNEVENT_UNKNOWN) {
		PyErr_SetString(TFS_ERROR, "Failed to get dynevent info.");
		return NULL;
	}

	ret = event_is_enabled(instance, TC_SYS, evt_name);
	free(evt_name);

	return ret;
}

static enum tracefs_hist_key_type hist_key_type(PyObject * py_type)
{
	int type = -1;

	if (PyUnicode_Check(py_type)) {
		const char *type_str = PyUnicode_DATA(py_type);

		if (lax_cmp(type_str, "normal") ||
		    lax_cmp(type_str, "n") )
			type = TRACEFS_HIST_KEY_NORMAL;
		else if (lax_cmp(type_str, "hex") ||
			 lax_cmp(type_str, "h") )
			type = TRACEFS_HIST_KEY_HEX;
		else if (lax_cmp(type_str, "sym"))
			 type = TRACEFS_HIST_KEY_SYM;
		else if (lax_cmp(type_str, "sym_offset") ||
			 lax_cmp(type_str, "so"))
			type = TRACEFS_HIST_KEY_SYM_OFFSET;
		else if (lax_cmp(type_str, "syscall") ||
			 lax_cmp(type_str, "sc"))
			type = TRACEFS_HIST_KEY_SYSCALL;
		else if (lax_cmp(type_str, "execname") ||
			 lax_cmp(type_str, "e"))
			 type = TRACEFS_HIST_KEY_EXECNAME;
		else if (lax_cmp(type_str, "log") ||
			 lax_cmp(type_str, "l"))
			type = TRACEFS_HIST_KEY_LOG;
		else if (lax_cmp(type_str, "users") ||
			 lax_cmp(type_str, "u"))
			type = TRACEFS_HIST_KEY_USECS;
		else if (lax_cmp(type_str, "max") ||
			 lax_cmp(type_str, "m"))
			type = TRACEFS_HIST_KEY_MAX;
		else {
			TfsError_fmt(NULL, "Unknown axis type %s\n",
				     type_str);
		}
	} else if (PyLong_CheckExact(py_type)) {
		type = PyLong_AsLong(py_type);
	} else {
		TfsError_fmt(NULL, "Unknown axis type %s\n");
	}

	return type;
}

static struct tracefs_hist *hist_from_key(struct tep_handle *tep,
					  const char *system, const char *event,
					  PyObject *py_key, PyObject * py_type)
{
	struct tracefs_hist *hist = NULL;
	int type = 0;

	if (PyUnicode_Check(py_key)) {
		if (py_type) {
			type = hist_key_type(py_type);
			if (type < 0)
				return NULL;
		}

		hist = tracefs_hist_alloc(tep, system, event,
					  PyUnicode_DATA(py_key), type);
	} else if (PyList_CheckExact(py_key)) {
		int i, n_keys = PyList_Size(py_key);
		struct tracefs_hist_axis *axes;
		PyObject *item_type;

		if (py_type) {
			/*
			 * The format of the types have to match the format
			 * of the keys.
			 */
			if (!PyList_CheckExact(py_key) ||
			    PyList_Size(py_type) != n_keys)
				return NULL;
		}

		axes = calloc(n_keys + 1, sizeof(*axes));
		if (!axes) {
			MEM_ERROR
			return NULL;
		}

		for (i = 0; i < n_keys; ++i) {
			axes[i].key = str_from_list(py_key, i);
			if (!axes[i].key)
				return NULL;

			if (py_type) {
				item_type = PyList_GetItem(py_type, i);
				if (!PyLong_CheckExact(item_type))
					return NULL;

				type = hist_key_type(item_type);
				if (type < 0)
					return NULL;

				axes[i].type = type;
			}
		}

		hist = tracefs_hist_alloc_nd(tep, system, event, axes);
		free(axes);
	}

	return hist;
}

static struct tracefs_hist *hist_from_axis(struct tep_handle *tep,
					   const char *system, const char *event,
					   PyObject *py_axes)
{
	struct tracefs_hist *hist = NULL;
	struct tracefs_hist_axis *axes;
	PyObject *key, *value;
	Py_ssize_t i = 0;
	int n_axes;

	n_axes = PyDict_Size(py_axes);
	if (PyErr_Occurred())
		return NULL;

	axes = calloc(n_axes + 1, sizeof(*axes));
	if (!axes) {
		MEM_ERROR
		return NULL;
	}

	while (PyDict_Next(py_axes, &i, &key, &value)) {
		axes[i - 1].key = PyUnicode_DATA(key);
		axes[i - 1].type = hist_key_type(value);
		if (PyErr_Occurred()) {
			PyErr_Print();
			free(axes);
			return NULL;
		}
	}

	hist = tracefs_hist_alloc_nd(tep, system, event, axes);
	free(axes);

	return hist;
}

PyObject *PyFtrace_hist(PyObject *self, PyObject *args,
					PyObject *kwargs)
{
	static char *kwlist[] =
		{"system", "event", "key", "type", "axes", "name", NULL};
	PyObject *py_axes = NULL, *py_key = NULL, *py_type = NULL;
	const char *system, *event, *name = NULL;
	struct tracefs_hist *hist = NULL;
	struct tep_handle *tep;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ss|OOOs",
					 kwlist,
					 &system,
					 &event,
					 &py_key,
					 &py_type,
					 &py_axes,
					 &name)) {
		return NULL;
	}

	tep = get_tep(NULL, NULL);
	if (!tep)
		return NULL;

	if (py_key && ! py_axes) {
		hist = hist_from_key(tep, system, event, py_key, py_type);
	} else if (!py_key && py_axes) {
		hist = hist_from_axis(tep, system, event, py_axes);
	} else {
		TfsError_setstr(NULL, "'key' or 'axis' must be provided.");
		return NULL;
	}

	if (!hist)
		goto fail;

	if (name && tracefs_hist_add_name(hist, name) < 0)
		goto fail;

	return PyTraceHist_New(hist);

 fail:
	TfsError_fmt(NULL, "Failed to create histogram for %s/%s",
		     system, event);
	tracefs_hist_free(hist);
	return NULL;
}

PyObject *PyFtrace_synth(PyObject *self, PyObject *args,
					 PyObject *kwargs)
{
	const char *name, *start_match, *end_match, *match_name=NULL;
	const char *start_sys, *start_evt, *end_sys, *end_evt;
	static char *kwlist[] = {"name",
				 "start_sys",
				 "start_evt",
				 "end_sys",
				 "end_evt",
				 "start_match",
				 "end_match",
				 "match_name",
				 NULL};
	static struct tracefs_synth *synth;
	struct tep_handle *tep;
	PyObject *py_synth;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "sssssss|s",
					 kwlist,
					 &name,
					 &start_sys,
					 &start_evt,
					 &end_sys,
					 &end_evt,
					 &start_match,
					 &end_match,
					 &match_name)) {
		return NULL;
	}

	tep = get_tep(NULL, NULL);
	if (!tep)
		return NULL;

	synth = tracefs_synth_alloc(tep, name,
				    start_sys, start_evt,
				    end_sys, end_evt,
				    start_match, end_match,
				    match_name);
	tep_free(tep);
	if (!synth) {
		MEM_ERROR;
		return NULL;
	}

	py_synth = PySynthEvent_New(synth);
	/*
	 * Here we only allocated and initializes a synthetic event object.
	 * However, no synthetic event is added to the system yet. Hence,
	 * there is no need to 'destroy' this event at exit.
	 */
	set_destroy_flag(py_synth, false);
	return py_synth;
}

PyObject *PyFtrace_set_ftrace_loglevel(PyObject *self, PyObject *args,
						       PyObject *kwargs)
{
	static char *kwlist[] = {"level", NULL};
	int level;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "i",
					 kwlist,
					 &level)) {
		return NULL;
	}

	if (level > TEP_LOG_ALL)
		level = TEP_LOG_ALL;

	if (level < 0)
		level = 0;

	tracefs_set_loglevel(level);
	tep_set_loglevel(level);

	Py_RETURN_NONE;
}

static bool set_fork_options(struct tracefs_instance *instance, bool enable)
{
	if (enable) {
		if (tracefs_option_enable(instance, TRACEFS_OPTION_EVENT_FORK) < 0 ||
		    tracefs_option_enable(instance, TRACEFS_OPTION_FUNCTION_FORK) < 0)
			return false;
	} else {
		if (tracefs_option_disable(instance, TRACEFS_OPTION_EVENT_FORK) < 0 ||
		    tracefs_option_disable(instance, TRACEFS_OPTION_FUNCTION_FORK) < 0)
			return false;
	}

	return true;
}

static bool hook2pid(struct tracefs_instance *instance, PyObject *pid_val, int fork)
{
	if (!set_pid(instance, "set_ftrace_pid", pid_val) ||
	    !set_pid(instance, "set_event_pid", pid_val))
		goto fail;

	if (fork < 0)
		return true;

	if (!set_fork_options(instance, fork))
		goto fail;

	return true;

 fail:
	TfsError_setstr(instance, "Failed to hook to PID");
	PyErr_Print();
	return false;
}

static void start_tracing_procces(struct tracefs_instance *instance,
				  char *const *argv,
				  char *const *envp)
{
	PyObject *pid_val = PyList_New(1);

	PyList_SET_ITEM(pid_val, 0, PyLong_FromLong(getpid()));
	if (!hook2pid(instance, pid_val, true))
		exit(1);

	tracing_ON(instance);
	if (execvpe(argv[0], argv, envp) < 0) {
		TfsError_fmt(instance, "Failed to exec \'%s\'",
			     argv[0]);
	}

	exit(1);
}

static PyObject *get_callback_func(const char *plugin_name, const char * py_callback)
{
	PyObject *py_name, *py_module, *py_func;

	py_name = PyUnicode_FromString(plugin_name);
	py_module = PyImport_Import(py_name);
	if (!py_module) {
		PyErr_Format(TRACECRUNCHER_ERROR,
			     "Failed to import plugin \'%s\'",
			     plugin_name);
		return NULL;
	}

	py_func = PyObject_GetAttrString(py_module, py_callback);
	if (!py_func || !PyCallable_Check(py_func)) {
		PyErr_Format(TRACECRUNCHER_ERROR,
			     "Failed to import callback from \'%s\'",
			     plugin_name);
		return NULL;
	}

	return py_func;
}

struct callback_context {
	void	*py_callback;

	bool	status;
} callback_ctx;

static int callback(struct tep_event *event, struct tep_record *record,
		    int cpu, void *ctx_ptr)
{
	struct callback_context *ctx = ctx_ptr;
	PyObject *ret;

	record->cpu = cpu; // Remove when the bug in libtracefs is fixed.

	PyObject *py_tep_event = PyTepEvent_New(event);
	PyObject *py_tep_record = PyTepRecord_New(record);

	PyObject *arglist = PyTuple_New(2);
	PyTuple_SetItem(arglist, 0, py_tep_event);
	PyTuple_SetItem(arglist, 1, py_tep_record);

	ret = PyObject_CallObject((PyObject *)ctx->py_callback, arglist);
	Py_DECREF(arglist);

	if (!ret) {
		if (PyErr_Occurred()) {
			PyObject *err_type, *err_value, *err_traceback;

			PyErr_Fetch(&err_type, &err_value, &err_traceback);
			if (err_type == PyExc_SystemExit) {
				if (PyLong_CheckExact(err_value))
					Py_Exit(PyLong_AsLong(err_value));
				else
					Py_Exit(0);
			} else {
				PyErr_Restore(err_type, err_value, err_traceback);
				PyErr_Print();
			}
		}

		goto stop;
	}

	if (PyLong_CheckExact(ret) && PyLong_AsLong(ret) != 0) {
		Py_DECREF(ret);
		goto stop;
	}

	Py_DECREF(ret);
	return 0;

 stop:
	ctx->status = false;
	return 1;
}

static bool notrace_this_pid(struct tracefs_instance *instance)
{
	int pid = getpid();

	if (!pid2file(instance, "set_ftrace_notrace_pid", pid, true) ||
	    !pid2file(instance, "set_event_notrace_pid", pid, true)) {
		TfsError_setstr(instance,
			        "Failed to desable tracing for \'this\' process.");
		return false;
	}

	return true;
}

static void iterate_raw_events_waitpid(struct tracefs_instance *instance,
				       struct tep_handle *tep,
				       PyObject *py_func,
				       pid_t pid)
{
	bool *callback_status = &callback_ctx.status;
	int ret;

	callback_ctx.py_callback = py_func;
	(*(volatile bool *)callback_status) = true;
	do {
		ret = tracefs_iterate_raw_events(tep, instance, NULL, 0,
						 callback, &callback_ctx);

		if (*(volatile bool *)callback_status == false || ret < 0)
			break;
	} while (waitpid(pid, NULL, WNOHANG) != pid);
}

static bool init_callback_tep(struct tracefs_instance *instance,
			      const char *plugin,
			      const char *py_callback,
			      struct tep_handle **tep,
			      PyObject **py_func)
{
	*py_func = get_callback_func(plugin, py_callback);
	if (!*py_func)
		return false;

	*tep = get_tep(tracefs_instance_get_dir(instance), NULL);
	if (!*tep)
		return false;

	if (!notrace_this_pid(instance))
		return false;

	return true;
}

PyObject *PyFtrace_trace_shell_process(PyObject *self, PyObject *args,
						       PyObject *kwargs)
{
	const char *plugin = "__main__", *py_callback = "callback";
	static char *kwlist[] = {"process", "plugin", "callback", "instance", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	struct tep_handle *tep;
	PyObject *py_func;
	char *process;
	pid_t pid;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|ssO",
					 kwlist,
					 &process,
					 &plugin,
					 &py_callback,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (!init_callback_tep(instance, plugin, py_callback, &tep, &py_func))
		return NULL;

	pid = fork();
	if (pid < 0) {
		PyErr_SetString(TRACECRUNCHER_ERROR, "Failed to fork");
		return NULL;
	}

	if (pid == 0) {
		char *argv[] = {getenv("SHELL"), "-c", process, NULL};
		char *envp[] = {NULL};

		start_tracing_procces(instance, argv, envp);
	}

	iterate_raw_events_waitpid(instance, tep, py_func, pid);

	Py_RETURN_NONE;
}

PyObject *PyFtrace_trace_process(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	const char *plugin = "__main__", *py_callback = "callback";
	static char *kwlist[] = {"argv", "plugin", "callback", "instance", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	struct tep_handle *tep;
	PyObject *py_func, *py_argv, *py_arg;
	pid_t pid;
	int i, argc;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|ssO",
					 kwlist,
					 &py_argv,
					 &plugin,
					 &py_callback,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (!init_callback_tep(instance, plugin, py_callback, &tep, &py_func))
		return NULL;

	if (!PyList_CheckExact(py_argv)) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Failed to parse \'argv\' list");
		return NULL;
	}

	argc = PyList_Size(py_argv);

	pid = fork();
	if (pid < 0) {
		PyErr_SetString(TRACECRUNCHER_ERROR, "Failed to fork");
		return NULL;
	}

	if (pid == 0) {
		char *argv[argc + 1];
		char *envp[] = {NULL};

		for (i = 0; i < argc; ++i) {
			py_arg = PyList_GetItem(py_argv, i);
			if (!PyUnicode_Check(py_arg))
				return NULL;

			argv[i] = PyUnicode_DATA(py_arg);
		}
		argv[argc] = NULL;
		start_tracing_procces(instance, argv, envp);
	}

	iterate_raw_events_waitpid(instance, tep, py_func, pid);

	Py_RETURN_NONE;
}

static struct tracefs_instance *pipe_instance;

static void pipe_stop(int sig)
{
	tracefs_trace_pipe_stop(pipe_instance);
}

PyObject *PyFtrace_read_trace(PyObject *self, PyObject *args,
					      PyObject *kwargs)
{
	signal(SIGINT, pipe_stop);

	if (!get_instance_from_arg(args, kwargs, &pipe_instance) ||
	    !notrace_this_pid(pipe_instance))
		return NULL;

	tracing_ON(pipe_instance);
	if (tracefs_trace_pipe_print(pipe_instance, 0) < 0) {
		TfsError_fmt(pipe_instance,
			     "Unable to read trace data from instance \'%s\'.",
			     get_instance_name(pipe_instance));
		return NULL;
	}

	signal(SIGINT, SIG_DFL);
	Py_RETURN_NONE;
}

struct tracefs_instance *itr_instance;
static bool iterate_keep_going;

static void iterate_stop(int sig)
{
	iterate_keep_going = false;
	tracefs_trace_pipe_stop(itr_instance);
}

PyObject *PyFtrace_iterate_trace(PyObject *self, PyObject *args,
					         PyObject *kwargs)
{
	static char *kwlist[] = {"plugin", "callback", "instance", NULL};
	const char *plugin = "__main__", *py_callback = "callback";
	bool *callback_status = &callback_ctx.status;
	bool *keep_going = &iterate_keep_going;
	PyObject *py_inst = NULL;
	struct tep_handle *tep;
	PyObject *py_func;
	int ret;

	(*(volatile bool *)keep_going) = true;
	signal(SIGINT, iterate_stop);

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|ssO",
					 kwlist,
					 &plugin,
					 &py_callback,
					 &py_inst)) {
		return NULL;
	}

	py_func = get_callback_func(plugin, py_callback);
	if (!py_func ||
	    !get_optional_instance(py_inst, &itr_instance) ||
	    !notrace_this_pid(itr_instance))
		return NULL;

	tep = get_tep(tracefs_instance_get_dir(itr_instance), NULL);
	if (!tep)
		return NULL;

	(*(volatile bool *)callback_status) = true;
	callback_ctx.py_callback = py_func;
	tracing_ON(itr_instance);

	while (*(volatile bool *)keep_going) {
		ret = tracefs_iterate_raw_events(tep, itr_instance, NULL, 0,
						 callback, &callback_ctx);

		if (*(volatile bool *)callback_status == false || ret < 0)
			break;
	}

	signal(SIGINT, SIG_DFL);
	Py_RETURN_NONE;
}

PyObject *PyFtrace_hook2pid(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"pid", "fork", "instance", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	PyObject *pid_val;
	int fork = -1;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|pO",
					 kwlist,
					 &pid_val,
					 &fork,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (!hook2pid(instance, pid_val, fork))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyFtrace_error_log(PyObject *self, PyObject *args,
					     PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *ret = NULL;
	char *err_log;
	bool ok;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	err_log = tfs_error_log(instance, &ok);
	if (err_log) {
		ret = PyUnicode_FromString(err_log);
		free(err_log);
	} else if (ok) {
		ret = PyUnicode_FromString(TC_NIL_MSG);
	}

	return ret;
}

PyObject *PyFtrace_clear_error_log(PyObject *self, PyObject *args,
						   PyObject *kwargs)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	if (!tfs_clear_error_log(instance))
		return NULL;

	Py_RETURN_NONE;
}

void PyFtrace_at_exit(void)
{
	if (seq.buffer)
		trace_seq_destroy(&seq);
}
