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

		PyErr_Format(TEP_ERROR, "%s\ntfs_error: %s",
			     tc_err_log, tfs_err_log);

		tfs_clear_error_log(instance);
		free(tfs_err_log);
		free(tc_err_log);
	} else {
		PyErr_FormatV(TEP_ERROR, fmt, args);
		va_end(args);
	}
}

static void TfsError_setstr(struct tracefs_instance *instance,
			    const char *msg)
{
	char *tfs_err_log = tfs_error_log(instance, NULL);

	if (tfs_err_log) {
		PyErr_Format(TEP_ERROR, "%s\ntfs_error: %s", msg, tfs_err_log);
		tfs_clear_error_log(instance);
		free(tfs_err_log);
	} else {
		PyErr_SetString(TEP_ERROR, msg);
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

static const char **get_arg_list(PyObject *py_list)
{
	const char **argv = NULL;
	PyObject *arg_py;
	int i, n;

	if (!PyList_CheckExact(py_list))
		goto fail;

	n = PyList_Size(py_list);
	argv = calloc(n + 1, sizeof(*argv));
	for (i = 0; i < n; ++i) {
		arg_py = PyList_GetItem(py_list, i);
		if (!PyUnicode_Check(arg_py))
			goto fail;

		argv[i] = PyUnicode_DATA(arg_py);
	}

	return argv;

 fail:
	PyErr_SetString(TRACECRUNCHER_ERROR,
			"Failed to parse argument list.");
	free(argv);
	return NULL;
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

		tep = tracefs_local_events_system(dir_str, sys_names);
		free(sys_names);
	} else {
		tep = tracefs_local_events(dir_str);
	}

	if (!tep) {
		TfsError_fmt(NULL, "Failed to get local events from \'%s\'.",
			     dir_str);
		return NULL;
	}

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

static inline void trim_new_line(char *val)
{
	val[strlen(val) - 1] = '\0';
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

PyObject *PyFtrace_dir(PyObject *self)
{
	return PyUnicode_FromString(tracefs_tracing_dir());
}

PyObject *PyFtrace_detach(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"object", NULL};
	PyFtrace_Object_HEAD *obj_head;
	PyObject *py_obj = NULL;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O",
					 kwlist,
					 &py_obj)) {
		return false;
	}

	obj_head = (PyFtrace_Object_HEAD *)py_obj;
	obj_head->destroy = false;

	Py_RETURN_NONE;
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
	static char *kwlist[] = {"instance", "systems", "events", NULL};
	PyObject *system_list = NULL, *event_list = NULL, *system_event_list;
	const char **systems = NULL, **events = NULL;
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	char *file = NULL;
	int ret, s, e;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|OOO",
					 kwlist,
					 &py_inst,
					 &system_list,
					 &event_list)) {
		return false;
	}

	if (!get_optional_instance(py_inst, &instance))
		return false;

	if (!system_list && !event_list)
		return event_enable_disable(instance, NULL, NULL, enable);

	if (!system_list && event_list) {
		if (PyUnicode_Check(event_list) &&
		    is_all(PyUnicode_DATA(event_list))) {
			return event_enable_disable(instance, NULL, NULL, enable);
		} else {
			TfsError_setstr(instance,
					"Failed to enable events for unspecified system");
			return false;
		}
	}

	systems = get_arg_list(system_list);
	if (!systems) {
		TfsError_setstr(instance, "Inconsistent \"systems\" argument.");
		return false;
	}

	if (!event_list) {
		for (s = 0; systems[s]; ++s) {
			ret = event_enable_disable(instance, systems[s], NULL, enable);
			if (ret < 0)
				return false;
		}

		return true;
	}

	if (!PyList_CheckExact(event_list))
		goto fail_with_err;

	for (s = 0; systems[s]; ++s) {
		system_event_list = PyList_GetItem(event_list, s);
		if (!system_event_list || !PyList_CheckExact(system_event_list))
			goto fail_with_err;

		events = get_arg_list(system_event_list);
		if (!events)
			goto fail_with_err;

		for (e = 0; events[e]; ++e) {
			if (!event_enable_disable(instance, systems[s], events[e], enable))
				goto fail;
		}

		free(events);
		events = NULL;
	}

	free(systems);

	return true;

 fail_with_err:
	TfsError_setstr(instance, "Inconsistent \"events\" argument.");

 fail:
	free(systems);
	free(events);
	free(file);

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
					 "ssO|)",
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

struct ftracepy_kprobe {
	char *event;
	char *function;
	char *probe;
};

static bool register_kprobe(const char *event,
			    const char *function,
			    const char *probe)
{
	if (tracefs_kprobe_raw(TC_SYS, event, function, probe) < 0) {
		TfsError_fmt(NULL, "Failed to register kprobe \'%s\'.",
			     event);
		return false;
	}

	return true;
}

static bool register_kretprobe(const char *event,
			       const char *function,
			       const char *probe)
{
	if (tracefs_kretprobe_raw(TC_SYS, event, function, probe) < 0) {
		TfsError_fmt(NULL, "Failed to register kretprobe \'%s\'.",
			     event);
		return false;
	}

	return true;
}

static bool unregister_kprobe(const char *event)
{
	if (tracefs_kprobe_clear_probe(TC_SYS, event, true) < 0) {
		TfsError_fmt(NULL, "Failed to unregister kprobe \'%s\'.",
			     event);
		return false;
	}

	return true;
}

PyObject *PyKprobe_event(PyKprobe *self)
{
	return PyUnicode_FromString(self->ptrObj->event);
}

PyObject *PyKprobe_system(PyKprobe *self)
{
	return PyUnicode_FromString(TC_SYS);
}

PyObject *PyKprobe_function(PyKprobe *self)
{
	return PyUnicode_FromString(self->ptrObj->function);
}

PyObject *PyKprobe_probe(PyKprobe *self)
{
	return PyUnicode_FromString(self->ptrObj->probe);
}

void ftracepy_kprobe_destroy(struct ftracepy_kprobe *kp)
{
	unregister_kprobe(kp->event);
}

void ftracepy_kprobe_free(struct ftracepy_kprobe *kp)
{
	free(kp->event);
	free(kp->function);
	free(kp->probe);
	free(kp);
}

static struct ftracepy_kprobe *
kprobe_new(const char *event, const char *function, const char *probe,
	   bool retprobe)
{
	struct ftracepy_kprobe *new_kprobe;

	if (retprobe) {
		if (!register_kretprobe(event, function, probe))
			return NULL;
	} else {
		if (!register_kprobe(event, function, probe))
			return NULL;
	}

	new_kprobe = calloc(1, sizeof(*new_kprobe));
	if (!new_kprobe) {
		MEM_ERROR;
		unregister_kprobe(event);

		return NULL;
	}

	new_kprobe->event = strdup(event);
	new_kprobe->function = strdup(function);
	new_kprobe->probe = strdup(probe);
	if (!new_kprobe->event ||
	    !new_kprobe->function ||
	    !new_kprobe->probe) {
		MEM_ERROR;
		ftracepy_kprobe_free(new_kprobe);

		return NULL;
	}

	return new_kprobe;
}

PyObject *PyFtrace_register_kprobe(PyObject *self, PyObject *args,
						   PyObject *kwargs)
{
	static char *kwlist[] = {"event", "function", "probe", NULL};
	const char *event, *function, *probe;
	struct ftracepy_kprobe *kprobe;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "sss",
					 kwlist,
					 &event,
					 &function,
					 &probe)) {
		return NULL;
	}

	kprobe = kprobe_new(event, function, probe, false);
	if (!kprobe)
		return NULL;

	return PyKprobe_New(kprobe);
}

PyObject *PyFtrace_register_kretprobe(PyObject *self, PyObject *args,
						      PyObject *kwargs)
{
	static char *kwlist[] = {"event", "function", "probe", NULL};
	const char *event, *function, *probe = "$retval";
	struct ftracepy_kprobe *kprobe;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ss|s",
					 kwlist,
					 &event,
					 &function,
					 &probe)) {
		return NULL;
	}

	kprobe = kprobe_new(event, function, probe, true);
	if (!kprobe)
		return NULL;

	return PyKprobe_New(kprobe);
}

PyObject *PyKprobe_set_filter(PyKprobe *self, PyObject *args,
					      PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	const char *filter;
	char path[PATH_MAX];

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

	sprintf(path, "events/%s/%s/filter", TC_SYS, self->ptrObj->event);
	if (!write_to_file_and_check(instance, path, filter)) {
		TfsError_setstr(instance, "Failed to set kprobe filter.");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject *PyKprobe_clear_filter(PyKprobe *self, PyObject *args,
						PyObject *kwargs)
{
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;
	char path[PATH_MAX];

	static char *kwlist[] = {"instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|O",
					 kwlist,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	sprintf(path, "events/%s/%s/filter", TC_SYS, self->ptrObj->event);
	if (!write_to_file(instance, path, OFF)) {
		TfsError_setstr(instance, "Failed to clear kprobe filter.");
		return NULL;
	}

	Py_RETURN_NONE;
}

static bool enable_kprobe(PyKprobe *self, PyObject *args, PyObject *kwargs,
			  bool enable)
{
	static char *kwlist[] = {"instance", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|O",
					 kwlist,
					 &py_inst)) {
		return false;
	}

	if (!get_optional_instance(py_inst, &instance))
		return false;

	return event_enable_disable(instance, TC_SYS, self->ptrObj->event,
				    enable);
}

PyObject *PyKprobe_enable(PyKprobe *self, PyObject *args,
					  PyObject *kwargs)
{
	if (!enable_kprobe(self, args, kwargs, true))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyKprobe_disable(PyKprobe *self, PyObject *args,
					   PyObject *kwargs)
{
	if (!enable_kprobe(self, args, kwargs, false))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyKprobe_is_enabled(PyKprobe *self, PyObject *args,
					      PyObject *kwargs)
{
	static char *kwlist[] = {"instance", NULL};
	struct tracefs_instance *instance;
	PyObject *py_inst = NULL;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|O",
					 kwlist,
					 &py_inst)) {
		return NULL;
	}

	if (!get_optional_instance(py_inst, &instance))
		return NULL;

	return event_is_enabled(instance, TC_SYS, self->ptrObj->event);
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
		TfsError_fmt(NULL, "Failed to import plugin \'%s\'",
			     plugin_name);
		return NULL;
	}

	py_func = PyObject_GetAttrString(py_module, py_callback);
	if (!py_func || !PyCallable_Check(py_func)) {
		TfsError_fmt(NULL,
			     "Failed to import callback from plugin \'%s\'",
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

	if (ret) {
		Py_DECREF(ret);
	} else {
		if (PyErr_Occurred()) {
			if (PyErr_ExceptionMatches(PyExc_SystemExit)) {
				PyErr_Clear();
			} else {
				PyErr_Print();
			}
		}

		ctx->status = false;
	}

	return 0;
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
	callback_ctx.py_callback = py_func;
	do {
		tracefs_iterate_raw_events(tep, instance, NULL, 0,
					   callback, &callback_ctx);
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

	*tep = tracefs_local_events(tracefs_instance_get_dir(instance));
	if (!*tep) {
		TfsError_fmt(instance,
			     "Unable to get 'tep' event from instance \'%s\'.",
			     get_instance_name(instance));
		return false;
	}

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
		TfsError_setstr(instance, "Failed to fork");
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
		TfsError_setstr(instance, "Failed to parse \'argv\' list");
		return NULL;
	}

	argc = PyList_Size(py_argv);

	pid = fork();
	if (pid < 0) {
		TfsError_setstr(instance, "Failed to fork");
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

	tep = tracefs_local_events(tracefs_instance_get_dir(itr_instance));
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
}
