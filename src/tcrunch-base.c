// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright (C) 2022, VMware, Tzvetomir Stoyanov <tz.stoyanov@gmail.com>
 *
 */

#include "tcrunch-base.h"

/**
 * tc_str_from_list - Get string from a Python list
 * @py_list - Python object, must be of type list.
 * @i - Position into the @py_list
 *
 * This API gets string at position @i from python list @py_list
 *
 * Returns poiner to string, or NULL in case of an error. The returned
 * string must not be freed.
 */
const char *tc_str_from_list(PyObject *py_list, int i)
{
	PyObject *item = PyList_GetItem(py_list, i);

	if (!PyUnicode_Check(item))
		return NULL;

	return PyUnicode_DATA(item);
}


/**
 * tc_list_get_str - Get array of strings from a Python list object
 * @py_list - Python object, must be of type list.
 * @strings - Pointer to array of strings. The array is allocated in the API.
 * @size - Pointer to int, where the size of the array will be returned (optional).
 *
 * This API parses given Python list of strings, extracts the items and stores them
 * in an array of strings. The array is allocated in the API and returned in @strings.
 * It must be freed with free(). The last element of the array is NULL. If non NULL
 * @size is passed, the size of the array is returned in @size. Note that the strings
 * from the array must not be freed.
 *
 * Returns 0 on success, -1 in case of an error.
 */
int tc_list_get_str(PyObject *py_list, const char ***strings, int *size)
{
	const char **str = NULL;
	int i, n;

	if (!strings)
		goto fail;
	if (!PyList_CheckExact(py_list))
		goto fail;

	n = PyList_Size(py_list);
	if (n < 1)
		goto out;

	str = calloc(n + 1, sizeof(*str));
	if (!str)
		goto fail;
	for (i = 0; i < n; ++i) {
		str[i] = tc_str_from_list(py_list, i);
		if (!str[i])
			goto fail;
	}
out:
	*strings = str;
	if (size)
		*size = n;

	return 0;

 fail:
	free(str);
	return -1;
}

/**
 * tc_list_get_uint - Get array of unsigned integers from a Python list object
 * @py_list - Python object, must be of type list.
 * @array - Pointer to array of unsigned integers. The array is allocated in the API.
 * @size - Pointer to int, where the size of the array will be returned (optional).
 *
 * This API parses given Python list of longs, extracts the items and stores them
 * in an array of unsigned integers. The array is allocated in the API and returned
 * in @array. It must be freed with free(). If non NULL @size is passed, the size
 * of the array is returned in @size.
 *
 * Returns 0 on success, -1 in case of an error.
 */
int tc_list_get_uint(PyObject *py_list, unsigned long **array, int *size)
{
	unsigned long *arr = NULL;
	PyObject *item;
	int i, n;

	if (!array)
		goto fail;
	if (!PyList_CheckExact(py_list))
		goto fail;

	n = PyList_Size(py_list);
	if (n < 1)
		goto out;

	arr = calloc(n, sizeof(*arr));
	if (!arr)
		goto fail;
	for (i = 0; i < n; ++i) {

		item = PyList_GetItem(py_list, i);
		if (!PyLong_CheckExact(item))
			goto fail;
		arr[i] = PyLong_AsUnsignedLong(item);
		if (arr[1] == (unsigned long)-1 && PyErr_Occurred())
			goto fail;
	}

out:
	*array = arr;
	if (size)
		*size = n;

	return 0;

 fail:
	free(arr);
	return -1;
}
