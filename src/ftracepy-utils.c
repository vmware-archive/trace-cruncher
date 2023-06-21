// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

// C
#include <unistd.h>
#include <search.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <signal.h>
#include <semaphore.h>
#include <time.h>

// trace-cruncher
#include "tcrunch-base.h"
#include "ftracepy-utils.h"
#include "trace-obj-debug.h"

PyObject *TFS_ERROR;
PyObject *TEP_ERROR;
PyObject *TRACECRUNCHER_ERROR;

#define UPROBES_SYSTEM "tc_uprobes"

#define FTRACE_UPROBE		0x1
#define FTRACE_URETPROBE	0x2

struct fprobes_list {
	int size;
	int count;
	void **data;
};

struct utrace_func {
	int type;
	char *func_name;
	char *func_args;
};

struct py_utrace_context {
	pid_t pid;
	char **cmd_argv;
	char *usystem;
	uint32_t trace_time; /* in msec */
	struct fprobes_list fretprobes;
	struct fprobes_list ufuncs;
	struct fprobes_list uevents;
	struct dbg_trace_context *dbg;
};

static char *kernel_version()
{
	struct utsname uts;

	if (uname(&uts) != 0) {
		PyErr_SetString(TFS_ERROR, "Failed to get kernel version.");
		return NULL;
	}

	return strdup(uts.release);
}

static bool check_kernel_support(const char *api, int major, int minor)
{
	char *buff, *this_kernel = kernel_version();
	const char *dlm = ".";
	bool ret = false;
	int mj, mn;

        buff = strtok(this_kernel, dlm);
	mj = atoi(buff);
	if (mj > major)
		ret = true;

	if (mj == major) {
		buff = strtok(NULL, dlm);
		mn = atoi(buff);
		if (mn >= minor)
			ret = true;
	}

	free(this_kernel);
	if (!ret) {
		PyErr_Format(TFS_ERROR,
			     "Using \'%s()\' requires kernel versions >= %i.%i",
			     api, major, minor);
	}

	return ret;
}

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

		if (vasprintf(&tc_err_log, fmt, args) <= 0)
			goto out;

		PyErr_Format(TFS_ERROR, "%s\ntfs_error: %s",
			     tc_err_log, tfs_err_log);

		tfs_clear_error_log(instance);
		free(tc_err_log);
	} else {
		PyErr_FormatV(TFS_ERROR, fmt, args);
	}
out:
	free(tfs_err_log);
	va_end(args);
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

static struct tep_handle *get_tep(const char *dir, const char **sys_names)
{
	struct tep_handle *tep;

	tep = tracefs_local_events_system(dir, sys_names);
	if (!tep) {
		TfsError_fmt(NULL,
			     "Failed to get local 'tep' event from %s", dir?dir:"N/A");
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
		const char **sys_names = NULL;

		if (tc_list_get_str(system_list, &sys_names, NULL) || !sys_names) {
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
	if (ret < 0) {
		TfsError_fmt(NULL, "Failed to register handler for event %s/%s (%i).",
			     system, event, id);
		return NULL;
	}

	Py_RETURN_NONE;
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

PyObject *PyTfsInstance_reset(PyTfsInstance *self)
{
	tracefs_instance_reset(self->ptrObj);
	Py_RETURN_NONE;
}

int py_instance_destroy(struct tracefs_instance *instance)
{
	tracefs_instance_reset(instance);
	tracefs_instance_destroy(instance);
	return 0;
}

PyObject *PyTfsInstance_delete(PyTfsInstance *self)
{
	py_instance_destroy(self->ptrObj);

	Py_RETURN_NONE;
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
			key = tc_str_from_list(py_obj, i);
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

PyObject *PySynthEvent_register(PySynthEvent *self, PyObject *args, PyObject *kwargs)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	tracefs_synth_set_instance(self->ptrObj, instance);
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

PyObject *PySynthEvent_repr(PySynthEvent *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"event", "hist_start", "hist_end", NULL};
	int event, hist_start, hist_end;
	char buff[2048] = {0};
	bool new_line = false;
	const char *str = NULL;

	event = hist_start = hist_end = true;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|ppp",
					 kwlist,
					 &event,
					 &hist_start,
					 &hist_end)) {
		return NULL;
	}

	if (event) {
		strcat(buff, "synth. event: ");
		str = tracefs_synth_show_event(self->ptrObj);
		if (str)
			strcat(buff, str);
		new_line = true;
	}

	if (hist_start) {
		if (new_line)
			strcat(buff, "\n");
		else
			new_line = true;

		strcat(buff, "hist. start: ");
		str = tracefs_synth_show_start_hist(self->ptrObj);
		if (str)
			strcat(buff, str);
	}

	if (hist_end) {
		if (new_line)
			strcat(buff, "\n");

		strcat(buff, "hist. end: ");
		str = tracefs_synth_show_end_hist(self->ptrObj);
		if (str)
			strcat(buff, str);
	}

	return PyUnicode_FromString(strdup(buff));
}

PyObject *PyFtrace_dir(PyObject *self)
{
	return PyUnicode_FromString(tracefs_tracing_dir());
}

PyObject *PyFtrace_set_dir(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"path", NULL};
	char *custom_dir = NULL;
	int ret;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|s",
					 kwlist,
					 &custom_dir)) {
		return NULL;
	}

	if (custom_dir && custom_dir[0] != '\0')
		ret = tracefs_set_tracing_dir(custom_dir);
	else
		ret = tracefs_set_tracing_dir(NULL);

	if (ret) {
		TfsError_setstr(NULL, "Failed to set custom ftrace directory.");
		return NULL;
	}

	Py_RETURN_NONE;
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

static int save_instace(const char *name, void *context)
{
	PyObject **list = (PyObject **)context;
	struct tracefs_instance *instance;
	PyObject *py_inst;

	instance = tracefs_instance_alloc(NULL, name);
	if (instance) {
		py_inst = PyTfsInstance_New(instance);
		/* Do not destroy the instance in the system, as we did not create it */
		set_destroy_flag(py_inst, false);
		PyList_Append(*list, py_inst);
	}

	return 0;
}

PyObject *PyFtrace_available_instances(PyObject *self, PyObject *args,
						       PyObject *kwargs)
{
	PyObject *list;

	list = PyList_New(0);
	if (tracefs_instances_walk(save_instace, &list) < 0)
		return NULL;
	return list;
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

		if (n == 0 || (n == 1 && is_all(tc_str_from_list(py_events, 0)))) {
			if (!event_enable_disable(instance, system, NULL,
						  enable))
				return false;
			continue;
		}

		for (i = 0; i < n; ++i) {
			event = tc_str_from_list(py_events, i);
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
	const char *system, *filter;
	char *event = NULL;
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	char path[PATH_MAX];
	bool ret;

	static char *kwlist[] = {"system", "filter", "event", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ss|sO",
					 kwlist,
					 &system,
					 &filter,
					 &event,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (event) {
		sprintf(path, "events/%s/%s/filter", system, event);
		ret = write_to_file_and_check(instance, path, filter);
	} else {
		/*
		 * Filters written to system's filter file are not stored in this file.
		 * Instead, they are sent to each event's filter file from that system,
		 * which has the fields from the filter.
		 * That's why write_to_file_and_check() API cannot be used in this case.
		 */
		sprintf(path, "events/%s/filter", system);
		ret = (write_to_file(instance, path, filter) > 0);
	}

	if (!ret) {
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
	const char *system;
	char *event = NULL;
	char path[PATH_MAX];

	static char *kwlist[] = {"system", "event", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|sO",
					 kwlist,
					 &system,
					 &event,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	if (event)
		sprintf(path, "events/%s/%s/filter", system, event);
	else
		sprintf(path, "events/%s/filter", system);
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

PyObject *PyFtrace_no_arg(PyObject *self)
{
	return PyUnicode_FromString(NO_ARG);
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

PyObject *PyDynevent_register(PyDynevent *self)
{
	if (tracefs_dynevent_create(self->ptrObj) < 0) {
		char *evt;
		int type;

		type = tracefs_dynevent_info(self->ptrObj, NULL, &evt, NULL, NULL, NULL);
		TfsError_fmt(NULL, "Failed to register dynamic event '%s'.",
		type != TRACEFS_DYNEVENT_UNKNOWN ? evt : "UNKNOWN");
		free(evt);
		return NULL;
	}

	/*
	 * Here the dynamic event gets added to the system.
	 * Hence we need to 'destroy' this event at exit.
	 */
	set_destroy_flag((PyObject *)self, true);
	Py_RETURN_NONE;
}

PyObject *PyDynevent_unregister(PyDynevent *self)
{
	if (tracefs_dynevent_destroy(self->ptrObj, true) < 0) {
		char *evt;
		int type;

		type = tracefs_dynevent_info(self->ptrObj, NULL, &evt, NULL, NULL, NULL);
		TfsError_fmt(NULL, "Failed to unregister dynamic event '%s'.",
		type != TRACEFS_DYNEVENT_UNKNOWN ? evt : "UNKNOWN");
		free(evt);
		return NULL;
	}

	/*
	 * Here the synthetic event gets removed from the system.
	 * Hence we no loger need to 'destroy' this event at exit.
	 */
	set_destroy_flag((PyObject *)self, false);
	Py_RETURN_NONE;
}

PyObject *PyFtrace_kprobe(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"event", "function", "probe", NULL};
	const char *event, *function, *probe;
	struct tracefs_dynevent *kprobe;
	PyObject *py_dyn;

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

	py_dyn = PyDynevent_New(kprobe);
	/*
	 * Here we only allocated and initializes a dynamic event object.
	 * However, no dynamic event is added to the system yet. Hence,
	 * there is no need to 'destroy' this event at exit.
	 */
	set_destroy_flag(py_dyn, false);
	return py_dyn;
}

PyObject *PyFtrace_kretprobe(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"event", "function", "probe", NULL};
	const char *event, *function, *probe = "$retval";
	struct tracefs_dynevent *kprobe;
	PyObject *py_dyn;

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

	py_dyn = PyDynevent_New(kprobe);
	/*
	 * Here we only allocated and initializes a dynamic event object.
	 * However, no dynamic event is added to the system yet. Hence,
	 * there is no need to 'destroy' this event at exit.
	 */
	set_destroy_flag(py_dyn, false);
	return py_dyn;
}

struct tep_event *dynevent_get_event(PyDynevent *event,
				     struct tep_handle **tep_ptr)
{
	struct tep_event *tep_evt;
	struct tep_handle *tep;

	tep = get_tep(NULL, NULL);
	if (!tep)
		return NULL;

	tep_evt = tracefs_dynevent_get_event(tep, event->ptrObj);
	if (!tep_evt) {
		TfsError_setstr(NULL, "Failed to get dynevent.");
		return NULL;
	}

	if (tep_ptr)
		*tep_ptr = tep;

	return tep_evt;
}

PyObject *PyFtrace_eprobe(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"event", "target_system", "target_event", "fetch_fields", NULL};
	const char *event, *target_system, *target_event, *fetchargs;
	struct tracefs_dynevent *eprobe;
	PyObject *py_dyn;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ssss",
					 kwlist,
					 &event,
					 &target_system,
					 &target_event,
					 &fetchargs)) {
		return NULL;
	}

	/* 'eprobes' are introduced in kernel version 5.15. */
	if (!check_kernel_support("eprobe", 5, 15))
		return NULL;

	eprobe = tracefs_eprobe_alloc(TC_SYS, event, target_system, target_event, fetchargs);
	if (!eprobe) {
		MEM_ERROR;
		return NULL;
	}

	py_dyn = PyDynevent_New(eprobe);
	/*
	 * Here we only allocated and initializes a dynamic event object.
	 * However, no dynamic event is added to the system yet. Hence,
	 * there is no need to 'destroy' this event at exit.
	 */
	set_destroy_flag(py_dyn, false);
	return py_dyn;
}

static PyObject *alloc_uprobe(PyObject *self, PyObject *args, PyObject *kwargs, bool pret)
{
	static char *kwlist[] = {"event", "file", "offset", "fetch_args", NULL};
	const char *event, *file, *fetchargs = NULL;
	unsigned long long offset;
	struct tracefs_dynevent *uprobe;
	PyObject *py_dyn;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ssK|s",
					 kwlist,
					 &event,
					 &file,
					 &offset,
					 &fetchargs)) {
		return NULL;
	}

	if (pret)
		uprobe = tracefs_uretprobe_alloc(TC_SYS, event, file, offset, fetchargs);
	else
		uprobe = tracefs_uprobe_alloc(TC_SYS, event, file, offset, fetchargs);
	if (!uprobe) {
		MEM_ERROR;
		return NULL;
	}

	py_dyn = PyDynevent_New(uprobe);
	/*
	 * Here we only allocated and initializes a dynamic event object.
	 * However, no dynamic event is added to the system yet. Hence,
	 * there is no need to 'destroy' this event at exit.
	 */
	set_destroy_flag(py_dyn, false);
	return py_dyn;
}

PyObject *PyFtrace_uprobe(PyObject *self, PyObject *args, PyObject *kwargs)
{
	return alloc_uprobe(self, args, kwargs, false);
}

PyObject *PyFtrace_uretprobe(PyObject *self, PyObject *args, PyObject *kwargs)
{
	return alloc_uprobe(self, args, kwargs, true);
}

static PyObject *set_filter(PyObject *args, PyObject *kwargs,
			    struct tep_handle *tep,
			    struct tep_event *event)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
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

	if (tracefs_event_filter_apply(instance, event, filter) < 0) {
		TfsError_fmt(NULL, "Failed to apply filter '%s' to event '%s'.",
			     filter, event->name);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *get_filter(PyObject *args, PyObject *kwargs,
			    const char *system, const char* event )
{
	struct tracefs_instance *instance;
	PyObject *ret = NULL;
	char *filter = NULL;
	char path[PATH_MAX];

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	sprintf(path, "events/%s/%s/filter", system, event);
	if (read_from_file(instance, path, &filter) <= 0)
		return NULL;

	trim_new_line(filter);
	ret = PyUnicode_FromString(filter);
	free(filter);

	return ret;
}

static PyObject *clear_filter(PyObject *args, PyObject *kwargs,
			      struct tep_event *event)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	if (tracefs_event_filter_clear(instance, event) < 0) {
		TfsError_fmt(NULL, "Failed to clear filter for event '%s'.",
			     event->name);
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject *PyDynevent_set_filter(PyDynevent *self, PyObject *args,
						  PyObject *kwargs)
{
	struct tep_handle *tep;
	struct tep_event *evt;

	evt = dynevent_get_event(self, &tep);
	if (!evt)
		return NULL;

	return set_filter(args, kwargs, tep, evt);
}

PyObject *PyDynevent_get_filter(PyDynevent *self, PyObject *args,
						  PyObject *kwargs)
{
	struct tep_event *evt = dynevent_get_event(self, NULL);
	char *evt_name, *evt_system;
	int type;

	if (!evt)
		return NULL;

	type = tracefs_dynevent_info(self->ptrObj, &evt_system, &evt_name,
				     NULL, NULL, NULL);

	if (type == TRACEFS_DYNEVENT_UNKNOWN) {
		PyErr_SetString(TFS_ERROR, "Failed to get dynevent info.");
		return NULL;
	}

	return get_filter(args, kwargs, evt_system, evt_name);
}

PyObject *PyDynevent_clear_filter(PyDynevent *self, PyObject *args,
						    PyObject *kwargs)
{
	struct tep_event *evt = dynevent_get_event(self, NULL);

	if (!evt)
		return NULL;

	return clear_filter(args, kwargs, evt);
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
			axes[i].key = tc_str_from_list(py_key, i);
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

#define SYNTH_SYS "synthetic"

static bool enable_synth(PySynthEvent *self, PyObject *args, PyObject *kwargs,
			 bool enable)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	return event_enable_disable(instance, SYNTH_SYS,
				    tracefs_synth_get_name(self->ptrObj),
				    enable);
}

PyObject *PySynthEvent_enable(PySynthEvent *self, PyObject *args,
						  PyObject *kwargs)
{
	if (!enable_synth(self, args, kwargs, true))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PySynthEvent_disable(PySynthEvent *self, PyObject *args,
						   PyObject *kwargs)
{
	if (!enable_synth(self, args, kwargs, false))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PySynthEvent_is_enabled(PySynthEvent *self, PyObject *args,
						      PyObject *kwargs)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	return event_is_enabled(instance, SYNTH_SYS,
				tracefs_synth_get_name(self->ptrObj));
}

struct tep_event *synth_get_event(PySynthEvent *event, struct tep_handle **tep_ptr)
{
	struct tep_event *tep_evt;
	struct tep_handle *tep;

	tep = get_tep(NULL, NULL);
	if (!tep)
		return NULL;

	tep_evt = tracefs_synth_get_event(tep, event->ptrObj);
	if (!tep_evt) {
		TfsError_setstr(NULL, "Failed to get synth. event.");
		return NULL;
	}

	if (tep_ptr)
		*tep_ptr = tep;

	return tep_evt;
}

PyObject *PySynthEvent_set_filter(PySynthEvent *self, PyObject *args,
						      PyObject *kwargs)
{
	struct tep_handle *tep;
	struct tep_event *evt;

	evt = synth_get_event(self, &tep);
	if (!evt)
		return NULL;

	return set_filter(args, kwargs, tep, evt);
}

PyObject *PySynthEvent_get_filter(PySynthEvent *self, PyObject *args,
						      PyObject *kwargs)
{
	struct tep_event *evt = synth_get_event(self, NULL);

	if (!evt)
		return NULL;

	return get_filter(args, kwargs, SYNTH_SYS, evt->name);
}

PyObject *PySynthEvent_clear_filter(PySynthEvent *self, PyObject *args,
							PyObject *kwargs)
{
	struct tep_event *evt = synth_get_event(self, NULL);

	if (!evt)
		return NULL;

	return clear_filter(args, kwargs, evt);
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

#define UTRACE_LIST_INIT_SIZE	100
static int utrace_list_add(struct fprobes_list *list, void *data)
{
	void **tmp;
	int size;

	if (list->size <= list->count) {
		if (!list->size)
			size = UTRACE_LIST_INIT_SIZE;
		else
			size = 2 * list->size;
		tmp = realloc(list->data, size * sizeof(void *));
		if (!tmp)
			return -1;
		list->data = tmp;
		list->size = size;
	}

	list->data[list->count] = data;
	list->count++;
	return list->count - 1;
}

void py_utrace_free(struct py_utrace_context *utrace)
{
	struct utrace_func *f;
	int i;

	if (!utrace)
		return;
	if (utrace->dbg)
		dbg_trace_context_destroy(utrace->dbg);

	for (i = 0; i < utrace->ufuncs.count; i++) {
		f = utrace->ufuncs.data[i];
		free(f->func_name);
		free(f);
	}
	free(utrace->ufuncs.data);
	if (utrace->cmd_argv) {
		i = 0;
		while (utrace->cmd_argv[i])
			free(utrace->cmd_argv[i++]);
		free(utrace->cmd_argv);
	}

	for (i = 0; i < utrace->uevents.count; i++)
		tracefs_dynevent_free(utrace->uevents.data[i]);
	free(utrace->uevents.data);

	free(utrace->usystem);
	free(utrace);
}

/*
 * All strings, used as ftrace system or event name must contain only
 * alphabetic characters, digits or underscores.
 */
static void fname_unify(char *fname)
{
	int i;

	for (i = 0; fname[i]; i++)
		if (!isalnum(fname[i]) && fname[i] != '_')
			fname[i] = '_';
}

int py_utrace_destroy(struct py_utrace_context *utrace)
{
	int i;

	for (i = 0; i < utrace->uevents.count; i++)
		tracefs_dynevent_destroy(utrace->uevents.data[i], true);

	return 0;
}

static unsigned long long str_hash(char *s)
{
	unsigned long long sum = 0, add;
	int i, len = strlen(s);

	for (i = 0; i < len; i++) {
		if (i + 8 < len)
			add = *(long long *)(s+i);
		else if (i + 4 < len)
			add = *(long *)(s+i);
		else
			add = s[i];

		sum += add;
	}

	return sum;
}

static struct py_utrace_context *utrace_new(pid_t pid, char **argv, bool libs)
{
	struct py_utrace_context *utrace;

	utrace = calloc(1, sizeof(*utrace));
	if (!utrace)
		return NULL;

	if (argv) {
		utrace->cmd_argv = argv;
		utrace->dbg = dbg_trace_context_create_file(argv[0], libs);
		if (!utrace->dbg)
			goto error;
		if (asprintf(&utrace->usystem, "%s_%llX", UPROBES_SYSTEM, str_hash(argv[0])) <= 0)
			goto error;
	} else {
		utrace->pid = pid;
		utrace->dbg = dbg_trace_context_create_pid(pid, libs);
		if (!utrace->dbg)
			goto error;
		if (asprintf(&utrace->usystem, "%s_%d", UPROBES_SYSTEM, pid) <= 0)
			goto error;
	}

	fname_unify(utrace->usystem);
	return utrace;

error:
	py_utrace_free(utrace);
	return NULL;
}

static int py_utrace_add_func(struct py_utrace_context *utrace, char *func, int type)
{
	struct utrace_func *p;
	int ret;
	int i;

	for (i = 0; i < utrace->ufuncs.count; i++) {
		p = utrace->ufuncs.data[i];
		if (!strcmp(p->func_name, func)) {
			p->type |= type;
			return 0;
		}
	}

	p = calloc(1, sizeof(*p));
	if (!p)
		return -1;
	p->func_name = strdup(func);
	if (!p->func_name)
		goto error;
	p->type = type;

	ret = utrace_list_add(&utrace->ufuncs, p);
	if (ret < 0)
		goto error;

	if (dbg_trace_add_resolve_symbol(utrace->dbg, 0, func, ret))
		goto error;

	return 0;

error:
	free(p->func_name);
	free(p);
	return -1;
}

PyObject *PyUserTrace_add_function(PyUserTrace *self, PyObject *args,
				   PyObject *kwargs)
{
	struct py_utrace_context *utrace = self->ptrObj;
	static char *kwlist[] = {"fname", NULL};
	char *fname;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s",
					 kwlist,
					 &fname)) {
		return NULL;
	}

	if (py_utrace_add_func(utrace, fname, FTRACE_UPROBE) < 0) {
		MEM_ERROR
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject *PyUserTrace_add_ret_function(PyUserTrace *self, PyObject *args,
				       PyObject *kwargs)
{
	struct py_utrace_context *utrace = self->ptrObj;
	static char *kwlist[] = {"fname", NULL};
	char *fname;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s",
					 kwlist,
					 &fname)) {
		return NULL;
	}

	if (py_utrace_add_func(utrace, fname, FTRACE_URETPROBE) < 0) {
		MEM_ERROR
		return NULL;
	}

	Py_RETURN_NONE;
}

/*
 * Max event name is 64 bytes, hard coded in the kernel.
 * It can consist only of alphabetic characters, digits or underscores.
 */
#define FILENAME_TRUNCATE	10
#define FUNCAME_TRUNCATE	50
static char *uprobe_event_name(char *file, char *func, int type)
{
	char *event = NULL;
	char *fname;

	fname = strrchr(file, '/');
	if (fname)
		fname++;
	if (!fname || *fname == '\0')
		fname = file;

	if (asprintf(&event, "%s%.*s_%.*s", type == FTRACE_URETPROBE ? "r_":"",
		     FILENAME_TRUNCATE, fname, FUNCAME_TRUNCATE, func) <= 0) {
		return NULL;
	}

	fname_unify(event);

	return event;
}

/*
 * Create uprobe based on function name,
 * file name and function offset within the file.
 */
static int utrace_event_create(struct py_utrace_context *utrace,
			       struct dbg_trace_symbols *sym, char *fecthargs,
			       int type)
{
	struct tracefs_dynevent *uevent = NULL;
	char *rname;

	/* Generate uprobe event name, according to ftrace name requirements. */
	rname = uprobe_event_name(sym->fname, sym->name, type);
	if (!rname)
		return -1;

	if (type == FTRACE_URETPROBE)
		uevent = tracefs_uretprobe_alloc(utrace->usystem, rname,
						 sym->fname, sym->foffset, fecthargs);
	else
		uevent = tracefs_uprobe_alloc(utrace->usystem, rname,
					      sym->fname, sym->foffset, fecthargs);

	free(rname);
	if (!uevent)
		return -1;

	if (tracefs_dynevent_create(uevent)) {
		tracefs_dynevent_free(uevent);
		return -1;
	}

	utrace_list_add(&utrace->uevents, uevent);
	return 0;
}

/* A callback, called on each resolved function. */
static int symblos_walk(struct dbg_trace_symbols *sym, void *context)
{
	struct py_utrace_context *utrace = context;
	struct utrace_func *ufunc;

	if (!sym->name || !sym->fname || !sym->foffset ||
	    sym->cookie < 0 || sym->cookie >= utrace->ufuncs.count)
		return 0;

	ufunc = utrace->ufuncs.data[sym->cookie];

	if (ufunc->type & FTRACE_UPROBE)
		utrace_event_create(utrace, sym, ufunc->func_args, FTRACE_UPROBE);

	if (ufunc->type & FTRACE_URETPROBE)
		utrace_event_create(utrace, sym, ufunc->func_args, FTRACE_URETPROBE);

	return 0;
}

static void py_utrace_generate_uprobes(struct py_utrace_context *utrace)
{
	/* Find the exact name and file offset of each user function that should be traced. */
	dbg_trace_resolve_symbols(utrace->dbg);
	dbg_trace_walk_resolved_symbols(utrace->dbg, symblos_walk, utrace);
}

static int uprobe_start_trace(struct py_utrace_context *utrace, struct tracefs_instance *instance)
{
	PyObject *pPid = PyLong_FromLong(utrace->pid);
	bool ret;

	if (!pPid)
		return -1;

	/* Filter the trace only on desired pid(s). */
	ret = hook2pid(instance, PyLong_FromLong(utrace->pid), true);
	Py_DECREF(pPid);
	if (!ret) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Failed to set trace filter");
		return -1;
	}

	/* Enable uprobes in the system. */
	if (tracefs_event_enable(instance, utrace->usystem, NULL)) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Failed to enable trace events");
		return -1;
	}

	return 0;
}

#define PERF_EXEC_SYNC	"/TC_PERF_SYNC_XXXXXX"
static int uprobe_exec_cmd(struct py_utrace_context *utrace, struct tracefs_instance *instance)
{
	char *envp[] = {NULL};
	char sname[strlen(PERF_EXEC_SYNC) + 1];
	sem_t *sem;
	pid_t pid;
	int ret;

	strcpy(sname, PERF_EXEC_SYNC);
	mktemp(sname);
	sem = sem_open(sname, O_CREAT | O_EXCL, 0644, 0);
	sem_unlink(sname);

	pid = fork();
	if (pid < 0) {
		PyErr_SetString(TRACECRUNCHER_ERROR, "Failed to fork");
		return -1;
	}
	if (pid == 0) {
		sem_wait(sem);
		execvpe(utrace->cmd_argv[0], utrace->cmd_argv, envp);
	} else {
		utrace->pid = pid;
		uprobe_start_trace(utrace, instance);
		sem_post(sem);
		return ret;
	}

	return 0;
}

static int py_utrace_enable(struct py_utrace_context *utrace, struct tracefs_instance *instance)
{
	/* If uprobes on desired user functions are not yet generated, do it now. */
	if (!utrace->uevents.count)
		py_utrace_generate_uprobes(utrace);

	/* No functions are found in the given program / pid. */
	if (!utrace->uevents.count) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Cannot find requested user functions");
		return -1;
	}

	if (utrace->cmd_argv)
		uprobe_exec_cmd(utrace, instance);
	else
		uprobe_start_trace(utrace, instance);

	return 0;
}

static int py_utrace_disable(struct py_utrace_context *utrace, struct tracefs_instance *instance)
{
	/* Disable uprobes in the system. */
	if (tracefs_event_disable(instance, utrace->usystem, NULL)) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Failed to disable trace events");
		return -1;
	}

	return 0;
}

static bool tracing_run;

static void tracing_stop(int sig)
{
	tracing_run = false;
}

static void tracing_timer(int sig, siginfo_t *si, void *uc)
{
	tracing_run = false;
}

#define PID_WAIT_CHECK_USEC	500000
#define TIMER_SEC_NANO		1000000000LL
static int utrace_wait_pid(struct py_utrace_context *utrace)
{
	struct itimerspec tperiod = {0};
	struct sigaction saction = {0};
	struct sigevent stime = {0};
	timer_t timer_id;

	if (utrace->pid == 0)
		return -1;

	tracing_run = true;
	signal(SIGINT, tracing_stop);

	if (utrace->trace_time) {
		stime.sigev_notify = SIGEV_SIGNAL;
		stime.sigev_signo = SIGRTMIN;
		if (timer_create(CLOCK_MONOTONIC, &stime, &timer_id))
			return -1;
		saction.sa_flags = SA_SIGINFO;
		saction.sa_sigaction = tracing_timer;
		sigemptyset(&saction.sa_mask);
		if (sigaction(SIGRTMIN, &saction, NULL)) {
			timer_delete(timer_id);
			return -1;
		}
		/* Convert trace_time from msec to sec, nsec. */
		tperiod.it_value.tv_nsec = ((unsigned long long)utrace->trace_time * 1000000LL);
		if (tperiod.it_value.tv_nsec >= TIMER_SEC_NANO) {
			tperiod.it_value.tv_sec = tperiod.it_value.tv_nsec / TIMER_SEC_NANO;
			tperiod.it_value.tv_nsec %= TIMER_SEC_NANO;
		}
		if (timer_settime(timer_id, 0, &tperiod, NULL))
			return -1;
	}

	do {
		if (utrace->cmd_argv) { /* Wait for a child. */
			if (waitpid(utrace->pid, NULL, WNOHANG) == (int)utrace->pid) {
				utrace->pid = 0;
				tracing_run = false;
			}
		} else { /* Not a child, check if still exist. */
			if (kill(utrace->pid, 0) == -1 && errno == ESRCH) {
				utrace->pid = 0;
				tracing_run = false;
			}
		}
		usleep(PID_WAIT_CHECK_USEC);
	} while (tracing_run);

	if (utrace->trace_time)
		timer_delete(timer_id);

	signal(SIGINT, SIG_DFL);

	return 0;
}

PyObject *PyUserTrace_enable(PyUserTrace *self, PyObject *args, PyObject *kwargs)
{
	struct py_utrace_context *utrace = self->ptrObj;
	static char *kwlist[] = {"instance", "wait", "time", NULL};
	struct tracefs_instance *instance = NULL;
	PyObject *py_inst = NULL;
	int wait = false;
	int ret;

	if (!utrace) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Failed to get utrace context");
		return NULL;
	}

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|Ops",
					 kwlist,
					 &py_inst,
					 &wait,
					 &utrace->trace_time)) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Failed to parse input arguments");
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	ret = py_utrace_enable(utrace, instance);
	if (ret)
		return NULL;

	if (wait) {
		utrace_wait_pid(utrace);
		py_utrace_disable(utrace, instance);
	}

	Py_RETURN_NONE;
}

PyObject *PyUserTrace_disable(PyUserTrace *self, PyObject *args, PyObject *kwargs)
{
	struct py_utrace_context *utrace = self->ptrObj;
	static char *kwlist[] = {"instance", NULL};
	struct tracefs_instance *instance = NULL;
	PyObject *py_inst = NULL;
	int ret;

	if (!utrace) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Failed to get utrace context");
		return NULL;
	}

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|O",
					 kwlist,
					 &py_inst)) {
		PyErr_SetString(TRACECRUNCHER_ERROR,
				"Failed to parse input arguments");
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	ret = py_utrace_disable(utrace, instance);

	if (ret)
		return NULL;

	Py_RETURN_NONE;
}

static char *find_in_path(const char *name)
{
	char *paths = strdup(getenv("PATH"));
	char fullpath[PATH_MAX];
	bool found = false;
	char *tmp = paths;
	const char *item;

	if (!paths)
		return NULL;

	while ((item = strsep(&paths, ":")) != NULL) {
		snprintf(fullpath, PATH_MAX, "%s/%s", item, name);
		if (access(fullpath, F_OK|X_OK) == 0) {
			found = true;
			break;
		}
	}

	free(tmp);

	if (found)
		return strdup(fullpath);

	return NULL;
}

static char *get_full_name(char *name)
{
	bool resolve = false;
	char *fname, *tmp;

	if (!strchr(name, '/')) {
		tmp = find_in_path(name);
		if (!tmp)
			return NULL;
		resolve = true;
	} else {
		tmp = name;
	}

	fname = realpath(tmp, NULL);
	if (resolve)
		free(tmp);

	return fname;
}

PyObject *PyFtrace_user_trace(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"pid", "argv", "follow_libs", NULL};
	PyObject *py_utrace, *py_arg, *py_args = NULL;
	struct py_utrace_context *utrace;
	char **argv = NULL;
	long long pid = 0;
	int libs = 0;
	int i, argc;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|KOp",
					 kwlist,
					 &pid,
					 &py_args,
					 &libs)) {
		return NULL;
	}

	if (pid <= 0 && !py_args) {
		PyErr_Format(TFS_ERROR,
			     "Process ID or program name should be specified");
		return NULL;
	}
	if (pid > 0 && py_args) {
		PyErr_Format(TFS_ERROR,
			     "Only one of Process ID or program name should be specified");
		return NULL;
	}

	if (py_args) {
		if (!PyList_CheckExact(py_args)) {
			PyErr_Format(TFS_ERROR, "Failed to parse argv list");
			return NULL;
		}
		argc = PyList_Size(py_args);
		argv = calloc(argc + 1, sizeof(char *));
		for (i = 0; i < argc; i++) {
			py_arg = PyList_GetItem(py_args, i);
			if (!PyUnicode_Check(py_arg))
				continue;

			if (i == 0)
				argv[i] = get_full_name(PyUnicode_DATA(py_arg));
			else
				argv[i] = strdup(PyUnicode_DATA(py_arg));

			if (!argv[i]) {
				if (i == 0)
					PyErr_Format(TFS_ERROR,
						     "Failed to find program with name %s",
						     PyUnicode_DATA(py_arg));
				goto error;
			}
		}
		argv[i] = NULL;
	}

	utrace = utrace_new(pid, argv, libs);
	if (!utrace) {
		MEM_ERROR;
		goto error;
	}

	py_utrace = PyUserTrace_New(utrace);
	return py_utrace;

error:
	if (argv) {
		for (i = 0; i < argc; i++)
			free(argv[i]);
		free(argv);
	}
	return NULL;
}

PyObject *PyFtrace_available_dynamic_events(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"type", NULL};
	struct tracefs_dynevent **all;
	const char *type = NULL;
	unsigned int filter = 0;
	PyObject *list, *py_dyn;
	int i;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|s",
					 kwlist,
					 &type)) {
		return NULL;
	}

	if (type) {
		if (strstr(type, "kprobe"))
			filter |= TRACEFS_DYNEVENT_KPROBE;
		if (strstr(type, "kretprobe"))
			filter |= TRACEFS_DYNEVENT_KRETPROBE;
		if (strstr(type, "uprobe"))
			filter |= TRACEFS_DYNEVENT_UPROBE;
		if (strstr(type, "uretprobe"))
			filter |= TRACEFS_DYNEVENT_URETPROBE;
		if (strstr(type, "eprobe"))
			filter |= TRACEFS_DYNEVENT_EPROBE;
		if (strstr(type, "synthetic"))
			filter |= TRACEFS_DYNEVENT_SYNTH;
	}

	list = PyList_New(0);
	all = tracefs_dynevent_get_all(filter, NULL);
	if (all) {
		for (i = 0; all[i]; i++) {
			py_dyn = PyDynevent_New(all[i]);
			/* Do not destroy the event in the system, as we did not create it */
			set_destroy_flag(py_dyn, false);
			PyList_Append(list, py_dyn);
		}
		/* Do not free the list with tracefs_dynevent_list_free(),
		 * as it frees all items from the list. We need the dynamic events,
		 * only the list must be freed.
		 */
		free(all);
	}

	return list;
}

PyObject *PyFtrace_wait(PyObject *self, PyObject *args,
					PyObject *kwargs)
{
	static char *kwlist[] = {"signals", "pids", "kill", "time", NULL};
	PyObject *signals_list = NULL;
	const char **signals = NULL;
	const char *signals_default[] = {"SIGINT", "SIGTERM", NULL};
	PyObject *pids_list = NULL;
	unsigned long *pids = NULL;
	unsigned int time = 0;
	int npids = 0;
	int kill = 0;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|OOpI",
					 kwlist,
					 &signals_list,
					 &pids_list,
					 &kill,
					 &time)) {
		return NULL;
	}

	if (signals_list && tc_list_get_str(signals_list, &signals, NULL)) {
		TfsError_fmt(pipe_instance,
			     "Broken list of signals");
		goto error;
	}

	if (pids_list && tc_list_get_uint(pids_list, &pids, &npids)) {
		TfsError_fmt(pipe_instance,
			     "Broken list of PIDs");
		goto error;
	}

	tc_wait_condition(signals_list ? signals : signals_default,
			  pids, npids, kill, time, NULL, NULL);

	free(signals);
	free(pids);
	Py_RETURN_NONE;
error:
	free(signals);
	free(pids);
	return NULL;
}
