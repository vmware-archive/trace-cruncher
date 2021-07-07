// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
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

static void *instance_root;
PyObject *TFS_ERROR;
PyObject *TEP_ERROR;
PyObject *TRACECRUNCHER_ERROR;

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
	const char * name = self->ptrObj ? self->ptrObj->name : "nil";
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

	if (!field->size)
		return PyUnicode_FromString("(nil)");

	if (field->flags & TEP_FIELD_IS_STRING) {
		char *val_str = record->ptrObj->data + field->offset;
		return PyUnicode_FromString(val_str);
	} else if (is_number(field)) {
		unsigned long long val;

		tep_read_number_field(field, record->ptrObj->data, &val);
		return PyLong_FromLong(val);
	} else if (field->flags & TEP_FIELD_IS_POINTER) {
		void *val = record->ptrObj->data + field->offset;
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
			PyErr_SetString(TFS_ERROR,
					"Inconsistent \"systems\" argument.");
			return NULL;
		}

		tep = tracefs_local_events_system(dir_str, sys_names);
		free(sys_names);
	} else {
		tep = tracefs_local_events(dir_str);
	}

	if (!tep) {
		PyErr_Format(TFS_ERROR,
			     "Failed to get local events from \'%s\'.",
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
		PyErr_Format(TFS_ERROR, "File %s does not exist.", file);
		return false;
	}

	return true;
}

static bool check_dir(struct tracefs_instance *instance, const char *dir)
{
	if (!tracefs_dir_exists(instance, dir)) {
		PyErr_Format(TFS_ERROR, "Directory %s does not exist.", dir);
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
		PyErr_Format(TFS_ERROR,
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
		PyErr_Format(TFS_ERROR,
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
		PyErr_Format(TFS_ERROR, "Can not read from file %s", file);

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

static PyObject *tfs_list2py_list(char **list)
{
	PyObject *py_list = PyList_New(0);
	int i;

	for (i = 0; list && list[i]; i++)
		PyList_Append(py_list, PyUnicode_FromString(list[i]));

	tracefs_list_free(list);

	return py_list;
}

struct instance_wrapper {
	struct tracefs_instance *ptr;
	const char *name;
};

const char *instance_wrapper_get_name(const struct instance_wrapper *iw)
{
	if (!iw->ptr)
		return iw->name;

	return tracefs_instance_get_name(iw->ptr);
}

static int instance_compare(const void *a, const void *b)
{
	const struct instance_wrapper *iwa, *iwb;

	iwa = (const struct instance_wrapper *) a;
	iwb = (const struct instance_wrapper *) b;

	return strcmp(instance_wrapper_get_name(iwa),
		      instance_wrapper_get_name(iwb));
}

void instance_wrapper_free(void *ptr)
{
	struct instance_wrapper *iw;
	if (!ptr)
		return;

	iw = ptr;
	if (iw->ptr) {
		if (tracefs_instance_destroy(iw->ptr) < 0)
			fprintf(stderr,
				"\ntfs_error: Failed to destroy instance '%s'.\n",
				get_instance_name(iw->ptr));

		free(iw->ptr);
	}

	free(ptr);
}

static void destroy_all_instances(void)
{
	tdestroy(instance_root, instance_wrapper_free);
	instance_root = NULL;
}

static struct tracefs_instance *find_instance(const char *name)
{
	struct instance_wrapper iw, **iw_ptr;
	if (!is_set(name))
		return NULL;

	if (!tracefs_instance_exists(name)) {
		PyErr_Format(TFS_ERROR, "Trace instance \'%s\' does not exist.",
			     name);
		return NULL;
	}

	iw.ptr = NULL;
	iw.name = name;
	iw_ptr = tfind(&iw, &instance_root, instance_compare);
	if (!iw_ptr || !(*iw_ptr) || !(*iw_ptr)->ptr ||
	    strcmp(tracefs_instance_get_name((*iw_ptr)->ptr), name) != 0) {
		PyErr_Format(TFS_ERROR, "Unable to find trace instances \'%s\'.",
			     name);
		return NULL;
	}

	return (*iw_ptr)->ptr;
}

bool get_optional_instance(const char *instance_name,
			   struct tracefs_instance **instance)
{
	*instance = NULL;
	if (is_set(instance_name)) {
		*instance = find_instance(instance_name);
		if (!instance) {
			PyErr_Format(TFS_ERROR,
				     "Failed to find instance \'%s\'.",
				     instance_name);
			return false;
		}
	}

	return true;
}

bool get_instance_from_arg(PyObject *args, PyObject *kwargs,
			   struct tracefs_instance **instance)
{
	const char *instance_name;

	static char *kwlist[] = {"instance", NULL};
	instance_name = NO_ARG;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|s",
					 kwlist,
					 &instance_name)) {
		return false;
	}

	if (!get_optional_instance(instance_name, instance))
		return false;

	return true;
}

PyObject *PyFtrace_dir(PyObject *self)
{
	return PyUnicode_FromString(tracefs_tracing_dir());
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
	struct instance_wrapper *iw, **iw_ptr;
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
		PyErr_Format(TFS_ERROR,
			     "Failed to create new trace instance \'%s\'.",
			     name);
		return NULL;
	}

	iw = calloc(1, sizeof(*iw));
	if (!iw) {
		MEM_ERROR
		return NULL;
	}

	iw->ptr = instance;
	iw_ptr = tsearch(iw, &instance_root, instance_compare);
	if (!iw_ptr || !(*iw_ptr) || !(*iw_ptr)->ptr ||
	    strcmp(tracefs_instance_get_name((*iw_ptr)->ptr), name) != 0) {
		PyErr_Format(TFS_ERROR,
			     "Failed to store new trace instance \'%s\'.",
			     name);
		tracefs_instance_destroy(instance);
		tracefs_instance_free(instance);
		free(iw);

		return NULL;
	}

	if (!tracing_on)
		tracing_OFF(instance);

	return PyUnicode_FromString(name);
}

PyObject *PyFtrace_destroy_instance(PyObject *self, PyObject *args,
						    PyObject *kwargs)
{
	struct tracefs_instance *instance;
	struct instance_wrapper iw;
	char *name;

	static char *kwlist[] = {"name", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s",
					 kwlist,
					 &name)) {
		return NULL;
	}

	if (is_all(name)) {
		destroy_all_instances();
		Py_RETURN_NONE;
	}

	instance = find_instance(name);
	if (!instance) {
		PyErr_Format(TFS_ERROR,
			     "Unable to destroy trace instances \'%s\'.",
			     name);
		return NULL;
	}

	iw.ptr = NULL;
	iw.name = name;
	tdelete(&iw, &instance_root, instance_compare);

	tracefs_instance_destroy(instance);
	tracefs_instance_free(instance);

	Py_RETURN_NONE;
}

PyObject *instance_list;

static void instance_action(const void *nodep, VISIT which, int depth)
{
	struct instance_wrapper *iw = *( struct instance_wrapper **) nodep;
	const char *name;

	switch(which) {
	case preorder:
	case endorder:
		break;

	case postorder:
	case leaf:
		name = tracefs_instance_get_name(iw->ptr);
		PyList_Append(instance_list, PyUnicode_FromString(name));
		break;
	}
}

PyObject *PyFtrace_get_all_instances(PyObject *self)
{
	instance_list = PyList_New(0);
	twalk(instance_root, instance_action);

	return instance_list;
}

PyObject *PyFtrace_destroy_all_instances(PyObject *self)
{
	destroy_all_instances();

	Py_RETURN_NONE;
}

PyObject *PyFtrace_instance_dir(PyObject *self, PyObject *args,
						PyObject *kwargs)
{
	struct tracefs_instance *instance;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	return PyUnicode_FromString(tracefs_instance_get_dir(instance));
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

	return tfs_list2py_list(list);
}

PyObject *PyFtrace_set_current_tracer(PyObject *self, PyObject *args,
						      PyObject *kwargs)
{
	const char *file = "current_tracer", *tracer, *instance_name;
	struct tracefs_instance *instance;

	static char *kwlist[] = {"tracer", "instance", NULL};
	tracer = instance_name = NO_ARG;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|ss",
					 kwlist,
					 &tracer,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
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
			PyErr_Format(TFS_ERROR,
				     "Tracer \'%s\' is not available.",
				     tracer);
			return NULL;
		}
	} else if (!is_set(tracer)) {
		tracer = "nop";
	}

	if (!write_to_file_and_check(instance, file, tracer)) {
		PyErr_Format(TFS_ERROR, "Failed to enable tracer \'%s\'",
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
	struct tracefs_instance *instance;
	char **list;

	if (!get_instance_from_arg(args, kwargs, &instance))
		return NULL;

	list = tracefs_event_systems(tracefs_instance_get_dir(instance));
	if (!list)
		return NULL;

	return tfs_list2py_list(list);
}

PyObject *PyFtrace_available_system_events(PyObject *self, PyObject *args,
							   PyObject *kwargs)
{
	static char *kwlist[] = {"system", "instance", NULL};
	const char *instance_name = NO_ARG, *system;
	struct tracefs_instance *instance;
	char **list;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|s",
					 kwlist,
					 &system,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
		return NULL;

	list = tracefs_system_events(tracefs_instance_get_dir(instance),
				     system);
	if (!list)
		return NULL;

	return tfs_list2py_list(list);
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
	PyErr_Format(TFS_ERROR,
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
		PyErr_Format(TFS_ERROR,
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
	const char *instance_name, *system, *event;
	struct tracefs_instance *instance;

	instance_name = system = event = NO_ARG;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|sss",
					 kwlist,
					 &instance_name,
					 &system,
					 &event)) {
		return false;
	}

	if (!get_optional_instance(instance_name, &instance))
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
	const char *instance_name;
	char *file = NULL;
	int ret, s, e;

	instance_name = NO_ARG;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|sOO",
					 kwlist,
					 &instance_name,
					 &system_list,
					 &event_list)) {
		return false;
	}

	if (!get_optional_instance(instance_name, &instance))
		return false;

	if (!system_list && !event_list)
		return event_enable_disable(instance, NULL, NULL, enable);

	if (!system_list && event_list) {
		if (PyUnicode_Check(event_list) &&
		    is_all(PyUnicode_DATA(event_list))) {
			return event_enable_disable(instance, NULL, NULL, enable);
		} else {
			PyErr_SetString(TFS_ERROR,
					"Failed to enable events for unspecified system");
			return false;
		}
	}

	systems = get_arg_list(system_list);
	if (!systems) {
		PyErr_SetString(TFS_ERROR, "Inconsistent \"systems\" argument.");
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
	PyErr_SetString(TFS_ERROR, "Inconsistent \"events\" argument.");

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

PyObject *PyFtrace_event_is_enabled(PyObject *self, PyObject *args,
						    PyObject *kwargs)
{
	static char *kwlist[] = {"instance", "system", "event", NULL};
	const char *instance_name, *system, *event;
	struct tracefs_instance *instance;
	char *file, *val;
	PyObject *ret;

	instance_name = system = event = NO_ARG;
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "|sss",
					 kwlist,
					 &instance_name,
					 &system,
					 &event)) {
		return false;
	}

	if (!get_optional_instance(instance_name, &instance))
		return false;

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

PyObject *PyFtrace_set_event_filter(PyObject *self, PyObject *args,
						    PyObject *kwargs)
{
	const char *instance_name = NO_ARG, *system, *event, *filter;
	struct tracefs_instance *instance;
	char path[PATH_MAX];

	static char *kwlist[] = {"system", "event", "filter", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "sss|s",
					 kwlist,
					 &system,
					 &event,
					 &filter,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
		return NULL;

	sprintf(path, "events/%s/%s/filter", system, event);
	if (!write_to_file_and_check(instance, path, filter)) {
		PyErr_SetString(TFS_ERROR, "Failed to set event filter");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject *PyFtrace_clear_event_filter(PyObject *self, PyObject *args,
						      PyObject *kwargs)
{
	const char *instance_name = NO_ARG, *system, *event;
	struct tracefs_instance *instance;
	char path[PATH_MAX];

	static char *kwlist[] = {"system", "event", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "ss|s",
					 kwlist,
					 &system,
					 &event,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
		return NULL;

	sprintf(path, "events/%s/%s/filter", system, event);
	if (!write_to_file(instance, path, OFF)) {
		PyErr_SetString(TFS_ERROR, "Failed to clear event filter");
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

		PyErr_Format(TFS_ERROR,
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

		PyErr_Format(TFS_ERROR,
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

		PyErr_Format(TFS_ERROR,
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
	PyErr_Format(TFS_ERROR, "Failed to set PIDs for \"%s\"",
		     file);
	return false;
}

PyObject *PyFtrace_set_event_pid(PyObject *self, PyObject *args,
						 PyObject *kwargs)
{
	const char *instance_name = NO_ARG;
	struct tracefs_instance *instance;
	PyObject *pid_val;

	static char *kwlist[] = {"pid", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|s",
					 kwlist,
					 &pid_val,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
		return NULL;

	if (!set_pid(instance, "set_event_pid", pid_val))
		return NULL;

	Py_RETURN_NONE;
}

PyObject *PyFtrace_set_ftrace_pid(PyObject *self, PyObject *args,
						  PyObject *kwargs)
{
	const char *instance_name = NO_ARG;
	struct tracefs_instance *instance;
	PyObject *pid_val;

	static char *kwlist[] = {"pid", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|s",
					 kwlist,
					 &pid_val,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
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
		PyErr_Format(TFS_ERROR, "Failed to set option \"%s\"", opt);
		return false;
	}

	return true;
}

static PyObject *set_option_py_args(PyObject *args, PyObject *kwargs,
				   const char *val)
{
	const char *instance_name = NO_ARG, *opt;
	struct tracefs_instance *instance;

	static char *kwlist[] = {"option", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|s",
					 kwlist,
					 &opt,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
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
	const char *instance_name = NO_ARG, *opt;
	struct tracefs_instance *instance;
	enum tracefs_option_id opt_id;

	static char *kwlist[] = {"option", "instance", NULL};
	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "s|s",
					 kwlist,
					 &opt,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
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
	PyErr_SetString(TFS_ERROR, "Failed to hook to PID");
	PyErr_Print();
	return false;
}

PyObject *PyFtrace_hook2pid(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"pid", "fork", "instance", NULL};
	const char *instance_name = NO_ARG;
	struct tracefs_instance *instance;
	PyObject *pid_val;
	int fork = -1;

	if (!PyArg_ParseTupleAndKeywords(args,
					 kwargs,
					 "O|ps",
					 kwlist,
					 &pid_val,
					 &fork,
					 &instance_name)) {
		return NULL;
	}

	if (!get_optional_instance(instance_name, &instance))
		return NULL;

	if (!hook2pid(instance, pid_val, fork))
		return NULL;

	Py_RETURN_NONE;
}

void PyFtrace_at_exit(void)
{
	destroy_all_instances();
}
