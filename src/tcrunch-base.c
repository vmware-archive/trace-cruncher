// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright (C) 2022, VMware, Tzvetomir Stoyanov <tz.stoyanov@gmail.com>
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#include "tcrunch-base.h"

#define UNUSED(x) ((void)(x))

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

static bool tc_wait;

static void wait_stop(int sig)
{
	UNUSED(sig);
	tc_wait = false;
}

static void wait_timer(int sig, siginfo_t *si, void *uc)
{
	UNUSED(sig);
	UNUSED(si);
	UNUSED(uc);
	tc_wait = false;
}

struct {
	char *name;
	int sig;
} const signal_names[] = {
	{"SIGINT",	SIGINT},
	{"SIGTERM",	SIGTERM},
	{"SIGABRT",	SIGABRT},
	{"SIGALRM",	SIGALRM},
	{"SIGUSR1",	SIGUSR1},
	{"SIGUSR2",	SIGUSR2}
};

static int set_wait_signals(const char **signals, void (*sfunc)(int))
{
	static int sall = sizeof(signal_names)/sizeof(signal_names[0]);
	int j, i = 0;

	while (signals[i]) {
		for (j = 0; j < sall; j++) {
			if (strcasecmp(signals[i], signal_names[j].name) == 0) {
				signal(signal_names[j].sig, sfunc);
				break;
			}
		}
		if (j == sall)
			return -1;
		i++;
	}
	return 0;
}

struct user_job {
	int (*fjob)(void *c);
	void *context;
	bool completed;
};

static void *run_user_job(void *data)
{
	struct user_job *job = (struct user_job *)data;
	int old;

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old);

	if (job->fjob(job->context))
		tc_wait = false;

	job->completed = true;
	pthread_exit(NULL);
}

#define WAIT_CHECK_USEC	500000
#define TIMER_SEC_NANO	1000000000LL
/**
 * tc_wait_condition - Blocking wait on conditions
 * @signals - List of POSIX signals to wait for.
 * @pids - Array of PIDs to check.
 * @pidn - Size of @pids array.
 * @terminate - Flag whether to terminate @pids at exit.
 * @time - Time to wait, in msec .
 * @fjob - Function to be run in background, while waiting.
 * @context - Pointer to user context, will be passed to @fjob
 *
 * This API blocks until at least one of these conditions is met:
 *  1. If @signals is not NULL, track if any of these signals is received. Signals
 *     are described by its POSIX names. Supported are SIGINT, SIGTERM, SIGABRT,
 *     SIGALRM, SIGUSR1 and SIGUSR2.
 *  2. If @pids is not NULL and @pidn is greater than 0, wait until all PIDs from
 *     the array are alive. If @terminate is true and the function exits because
 *     any of the other condition is met, then PIDs from @pids which are still alive
 *     are killed.
 *  3. If @time is greater than 0, wait specified number of milliseconds before exit.
 *  4. If a user callback @fjob is provided, it will run in background. If @fjob returns
 *     a non zero value, the wait stops.
 * Any of these conditions is optional, but at least one must be provided.
 *
 * Returns 0 on success, -1 in case of an error.
 */
int tc_wait_condition(const char **signals, unsigned long *pids, int pidn, bool terminate,
		      unsigned long long time, int (*fjob)(void *), void *context)
{
	struct itimerspec tperiod = {0};
	struct sigaction saction = {0};
	struct sigevent stime = {0};
	unsigned long *sh_pids = NULL;
	bool signals_set = false;
	bool fjob_runs = false;
	bool timer_set = false;
	pthread_attr_t attrib;
	struct user_job job;
	pthread_t fthread;
	timer_t timer_id;
	int kids = 1;
	int ret = -1;
	int i;

	if (!signals && !pidn && !time && !fjob)
		return -1;

	/* Check if PIDs from the list are our children. */
	if (pidn > 0) {
		sh_pids = malloc(pidn * sizeof(*sh_pids));
		if (!sh_pids)
			goto out;
		memcpy(sh_pids, pids, pidn * sizeof(*sh_pids));
		if (waitpid(sh_pids[0], NULL, WNOHANG) == -1 && errno == ECHILD)
			kids = 0;
	}

	tc_wait = true;

	/* Init wait signals, if provided. */
	if (signals && *signals) {
		if (set_wait_signals(signals, wait_stop))
			goto out;
		signals_set = true;
	}

	/* Set a timer, if waiting time is provided. */
	if (time) {
		stime.sigev_notify = SIGEV_SIGNAL;
		stime.sigev_signo = SIGRTMIN;
		if (timer_create(CLOCK_MONOTONIC, &stime, &timer_id))
			goto out;
		timer_set = true;
		saction.sa_flags = SA_SIGINFO;
		saction.sa_sigaction = wait_timer;
		sigemptyset(&saction.sa_mask);
		if (sigaction(SIGRTMIN, &saction, NULL))
			goto out;
		/* Convert trace_time from msec to sec, nsec. */
		tperiod.it_value.tv_nsec = ((unsigned long long)time * 1000000LL);
		if (tperiod.it_value.tv_nsec >= TIMER_SEC_NANO) {
			tperiod.it_value.tv_sec = tperiod.it_value.tv_nsec / TIMER_SEC_NANO;
			tperiod.it_value.tv_nsec %= TIMER_SEC_NANO;
		}
		if (timer_settime(timer_id, 0, &tperiod, NULL))
			goto out;
	}

	/* Run the user callback in the background */
	if (fjob) {
		job.fjob = fjob;
		job.context = context;
		job.completed = false;
		pthread_attr_init(&attrib);
		pthread_attr_setdetachstate(&attrib, PTHREAD_CREATE_JOINABLE);
		if (pthread_create(&fthread, &attrib, run_user_job, &job))
			goto out;
		fjob_runs = true;
	}

	/* Wait for a condition loop. */
	do {
		if (pidn > 0) {
			for (i = 0; i < pidn; i++) {
				/* Check if any of the PIDs is still alive. */
				if (!sh_pids[i])
					continue;
				if (kids) { /* Wait for a child. */
					if (waitpid(sh_pids[i], NULL, WNOHANG) == (int)sh_pids[i])
						sh_pids[i] = 0;
				} else if (kill(sh_pids[i], 0) == -1 && errno == ESRCH) {
					/* Not a child, check if still exist. */
					sh_pids[i] = -1;
				}
				break;
			}
			if (i == pidn) {
				/* All PIDs from the array are terminated. */
				tc_wait = false;
				break;
			}
		}
		usleep(WAIT_CHECK_USEC);
	} while (tc_wait);

	ret = 0;

out:
	/* Clean up the timer. */
	if (timer_set)
		timer_delete(timer_id);

	/* Reset the signals to default value. */
	if (signals_set)
		set_wait_signals(signals, SIG_DFL);

	/* Kill the PIDs, if they still run.*/
	if (pidn > 0 && terminate) {
		for (i = 0; i < pidn; i++) {
			if (!sh_pids[i])
				continue;
			kill(sh_pids[i], SIGTERM);
		}
	}
	free(sh_pids);

	/* Kill the user job, if still running */
	if (fjob_runs) {
		pthread_attr_destroy(&attrib);
		if (!job.completed) {
			pthread_cancel(fthread);
			pthread_join(fthread, NULL);
		}
	}

	return ret;
}
