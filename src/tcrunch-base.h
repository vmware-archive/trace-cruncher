// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright (C) 2022, VMware, Tzvetomir Stoyanov <tz.stoyanov@gmail.com>
 *
 */

#ifndef _TC_BASE_H
#define _TC_BASE_H

#include <stdbool.h>

// Python
#define PY_SSIZE_T_CLEAN
#include <Python.h>

const char *tc_str_from_list(PyObject *py_list, int i);
int tc_list_get_str(PyObject *py_list, const char ***strings, int *size);
int tc_list_get_uint(PyObject *py_list, unsigned long **array, int *size);
int tc_wait_condition(const char **signals, unsigned long *pids, int pidn, bool terminate,
		      unsigned long long time, int (*fjob)(void *), void *context);

#endif /* _TC_BASE_H */
