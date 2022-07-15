// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright (C) 2022, VMware, Tzvetomir Stoyanov <tz.stoyanov@gmail.com>
 *
 */

/*
 * Helper program, used by some trace-cruncher unit tests to check specific functionality
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <libgen.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>

#define UNUSED(x) ((void)(x))
#define TIMER_SEC_NANO	1000000000LL

enum {
	OPT_runs	= 253,
	OPT_time	= 254,
	OPT_help	= 255,
};

void usage_help(char **argv)
{
	printf("\n %s: test program used by trace-cruncher unit tests. Usage:\n",
		basename(argv[0]));
	printf("\t --time, -t <msec> - Optional, run for given number of milliseconds.");
	printf("\t --runs, -r <count> - Optional, run the loop given number of runs.");
}

static int test_running;

static void run_timeout(int sig, siginfo_t *si, void *uc)
{
	UNUSED(sig);
	UNUSED(si);
	UNUSED(uc);
	test_running = 0;
}

void test_func3(int delay)
{
	usleep(delay);
}

void test_func2(int delay)
{
	test_func3(delay/2);
	usleep(delay/2);
}

void test_func1(int delay)
{
	test_func2(delay/2);
	usleep(delay/2);
}

#define RUN_STEP_SLEEP_USEC 50000
void main(int argc, char **argv)
{
	struct itimerspec tperiod = {0};
	struct sigaction saction = {0};
	struct sigevent stime = {0};
	int run_time = -1;
	int runs = -1;
	timer_t tid;
	int c;


	for (;;) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help", no_argument, NULL, OPT_help},
			{"time", required_argument, NULL, OPT_time},
			{"runs", required_argument, NULL, OPT_runs},
			{NULL, 0, NULL, 0}
		};

		c = getopt_long (argc, argv, "+ht:r:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 't':
		case OPT_time:
			run_time = atoi(optarg);
			break;
		case 'r':
		case OPT_runs:
			runs = atoi(optarg);
			break;
		case 'h':
		case OPT_help:
		default:
			usage_help(argv);
		}
	}

	if (run_time > 0) {
		stime.sigev_notify = SIGEV_SIGNAL;
		stime.sigev_signo = SIGRTMIN;
		if (timer_create(CLOCK_MONOTONIC, &stime, &tid))
			exit(1);
		saction.sa_flags = SA_SIGINFO;
		saction.sa_sigaction = run_timeout;
		sigemptyset(&saction.sa_mask);
		if (sigaction(SIGRTMIN, &saction, NULL))
			exit(1);
		/* Convert run_time from msec to sec, nsec. */
		tperiod.it_value.tv_nsec = ((unsigned long long)run_time * 1000000LL);
		if (tperiod.it_value.tv_nsec >= TIMER_SEC_NANO) {
			tperiod.it_value.tv_sec = tperiod.it_value.tv_nsec / TIMER_SEC_NANO;
			tperiod.it_value.tv_nsec %= TIMER_SEC_NANO;
		}
		if (timer_settime(tid, 0, &tperiod, NULL))
			exit(1);
	}
	test_running = 1;
	do {
		test_func1(RUN_STEP_SLEEP_USEC);
		if (runs > 0 && !(--runs))
			test_running = 0;
	} while (test_running);

	if (run_time > 0)
		timer_delete(tid);
	exit(0);
}
