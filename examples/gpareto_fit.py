#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
"""

import sys
import json

import matplotlib.pyplot as plt
import scipy.stats as st
import numpy as np

from scipy.stats import genpareto as gpareto
from scipy.optimize import curve_fit as cfit

from ksharksetup import setup
# Always call setup() before importing ksharkpy!!!
setup()

import ksharkpy as ks

def chi2_test(hist, n_bins, c, loc, scale, norm):
    """ Simple Chi^2 test for the goodness of the fit.
    """
    chi2 = n_empty_bins = 0
    for i in range(len(hist[0])):
        if hist[0][i] == 0:
            # Ignore this empty bin.
            n_empty_bins += 1
            continue

        # Get the center of bin i.
        x = (hist[1][i] + hist[1][i + 1]) / 2
        fit_val = gpareto.pdf(x, c=c, loc=loc, scale=scale)
        chi = (fit_val - hist[0][i]) / np.sqrt(hist[0][i])
        chi2 += chi**2

    return  norm * chi2 / (n_bins - n_empty_bins)

def quantile(p, P, c, loc, scale):
    """ The quantile function of the Generalized Pareto distribution.
    """
    return loc + scale / c * ((P / p)**(c) - 1)


def dq_dscale(p, P, c, scale):
    """ Partial derivative of the quantile function.
    """
    return ((P / p)**c - 1) / c


def dq_dc(p, P, c, scale):
    """ Partial derivative of the quantile function.
    """
    return (scale * (np.log(P / p) * (P / p)**c ) / c
          - scale * ((P / p)**c - 1) / (c**2))


def dq_dP(p, P, c, scale):
    """ Partial derivative of the quantile function.
    """
    return scale / P * (P / p)**c


def error_P(n, N):
    return np.sqrt(n) / N


def error(p, P, c, scale, err_P, err_c, err_scale):
    return np.sqrt((dq_dP(p, P, c, scale) * err_P)**2
                 + (dq_dc(p, P, c, scale) * err_c)**2
                 + (dq_dscale(p, P, c, scale) * err_scale)**2)


def quantile_conf_bound(p, P, n, c, loc, scale, err_P, err_c, err_scale):
    return (quantile(p=p, P=P, c=c, loc=loc, scale=scale)
          + n * error(p=p, P=P, c=c, scale=scale,
                      err_P=err_P, err_c=err_c, err_scale=err_scale));


def get_latency(t0, t1):
    """ Get the value of the latency in microseconds
    """
    return (t1 - t0) / 1000 - 1000


def get_cpu_data(data, task_pid, start_id, stop_id, threshold):
    """ Loop over the tracing data for a given CPU and find all latencies bigger
        than the specified threshold.
    """
    # Get the size of the data.
    size = ks.data_size(data)
    #print("data size:", size)

    time_start = -1
    dt_ot = []
    tot = 0
    i = 0
    i_start = 0;

    while i < size:
        if data["pid"][i] == task_pid and data['event'][i] == start_id:
            time_start = data['time'][i]
            i_start = i;
            i = i + 1

            while i < size:
                if data["pid"][i] == task_pid and data['event'][i] == stop_id:
                    delta = get_latency(time_start, data['time'][i])

                    if delta > threshold and tot != 0:
                        print('lat. over threshold: ', delta, i_start, i)
                        dt_ot.append([delta, i_start, i])               

                    tot = tot + 1
                    break

                i = i + 1
        i = i + 1

    print(task_pid, 'tot:', len(dt_ot), '/', tot)
    return dt_ot, tot


def make_ks_session(fname, data, start, stop):
    """ Save a KernelShark session descriptor file (Json).
        The sessions is zooming around the maximum observed latency.
    """
    sname = 'max_lat.json'
    ks.new_session(fname, sname)
    i_start = int(start)
    i_stop = int(stop)

    with open(sname, 'r+') as s:
        session = json.load(s)
        session['TaskPlots'] = [int(data['pid'][i_start])]
        session['CPUPlots'] = [int(data['cpu'][i_start])]

        delta = data['time'][i_stop] - data['time'][i_start]
        tmin = int(data['time'][i_start] - delta)
        tmax = int(data['time'][i_stop] + delta)
        session['Model']['range'] = [tmin, tmax]

        session['Markers']['markA']['isSet'] = True
        session['Markers']['markA']['row'] = i_start)

        session['Markers']['markB']['isSet'] = True
        session['Markers']['markB']['row'] = i_stop)

        session['ViewTop'] = i_start) - 5

        ks.save_session(session, s)


fname = str(sys.argv[1])
status = ks.open_file(fname)
if not status:
    print ("Failed to open file ", fname)
    sys.exit()

ks.register_plugin('reg_pid')
data = ks.load_data()

# Get the Event Ids of the hrtimer_start and print events.
start_id = ks.event_id('timer', 'hrtimer_start')
stop_id = ks.event_id('ftrace', 'print')
print("start_id", start_id)
print("stop_id", stop_id)

tasks = ks.get_tasks()
jdb_pids = tasks['jitterdebugger']
print('jitterdeburrer pids:', jdb_pids)
jdb_pids.pop(0)

threshold = 10
data_ot = []
tot = 0

for task_pid in jdb_pids:
    cpu_data, cpu_tot = get_cpu_data(data=data,
                                     task_pid=task_pid,
                                     start_id=start_id,
                                     stop_id=stop_id,
                                     threshold=threshold)

    data_ot.extend(cpu_data)
    tot += cpu_tot

ks.close()

dt_ot = np.array(data_ot)
np.savetxt('peak_over_threshold_loaded.txt', dt_ot)

make_ks_session(fname=fname, data=data, i_start=int(dt_ot[i_max_lat][1]),
                                        i_stop=int(dt_ot[i_max_lat][2]))

P = len(dt_ot) / tot
err_P = error_P(n=len(dt_ot), N=tot)
print('tot:', tot, ' P =', P)

lat = dt_ot[:,0]
#print(lat)
i_max_lat = lat.argmax()
print('imax:', i_max_lat, int(dt_ot[i_max_lat][1]))

print('max', np.amax(dt_ot))

start = threshold
stop = 31
n_bins = (stop - start) * 2

bin_size = (stop - start) / n_bins

x = np.linspace(start=start + bin_size / 2,
                stop=stop - bin_size / 2,
                num=n_bins)

bins_ot = np.linspace(start=start, stop=stop, num=n_bins + 1)
#print(bins_ot)

fig, ax = plt.subplots(nrows=2, ncols=2)
fig.tight_layout()
ax[-1, -1].axis('off')

hist_ot = ax[0][0].hist(x=lat, bins=bins_ot, histtype='stepfilled', alpha=0.3)
ax[0][0].set_xlabel('latency [\u03BCs]', fontsize=8)
ax[0][0].set_yscale('log')
#print(hist_ot[0])

hist_ot_norm = ax[1][0].hist(x=lat, bins=bins_ot,
                             density=True, histtype='stepfilled', alpha=0.3)

# Fit using the fitter of the genpareto class (shown in red).
ret = gpareto.fit(lat, loc=threshold)
ax[1][0].plot(x, gpareto.pdf(x, c=ret[0],  loc=ret[1],  scale=ret[2]),
              'r-', lw=1, color='red',  alpha=0.8)

ax[1][0].set_xlabel('latency [\u03BCs]', fontsize=8)
print(ret)
print('\ngoodness-of-fit: ' + '{:03.3f}'.format(chi2_test(hist_ot_norm,
                                                          n_bins=n_bins,
                                                          c=ret[0],
                                                          loc=ret[1],
                                                          scale=ret[2],
                                                          norm=len(lat))))

print("\n curve_fit:")
# Fit using the curve_fit fitter. Fix the value of the "loc" parameter.
popt, pcov = cfit(lambda x, c, scale: gpareto.pdf(x, c=c, loc=threshold, scale=scale),
                  x, hist_ot_norm[0],
                  p0=[ret[0], ret[2]])

print(popt)
print(pcov)

ax[1][0].plot(x, gpareto.pdf(x, c=popt[0], loc=threshold, scale=popt[1]),
              'r-', lw=1, color='blue', alpha=0.8)

fit_legend = str('\u03BE = ' + '{:05.3f}'.format(popt[0]) +
                 ' +- ' + '{:05.3f}'.format(pcov[0][0]**0.5) +
                 ' (' + '{:03.2f}'.format(pcov[0][0]**0.5 / abs(popt[0]) * 100) + '%)')

fit_legend += str('\n\u03C3 = ' + '{:05.3f}'.format(popt[1]) +
                  ' +- ' + '{:05.3f}'.format(pcov[1][1]**0.5) +
                  ' (' + '{:03.2f}'.format(pcov[1][1]**0.5 / abs(popt[1]) * 100) + '%)')

fit_legend += '\n\u03BC = ' + str(threshold) + ' (const)'

fit_legend += '\ngoodness-of-fit: ' + '{:03.3f}'.format(chi2_test(hist_ot_norm,
                                                        n_bins=n_bins,
                                                        c=popt[0],
                                                        loc=threshold,
                                                        scale=popt[1],
                                                        norm=len(lat)))
print(fit_legend)

ax[0][1].set_xscale('log')
##ax[0][1].set_yscale('log')
ax[0][1].set_xlabel('Return period', fontsize=8)
ax[0][1].set_ylabel('Return level [\u03BCs]', fontsize=6)
ax[0][1].grid(True, linestyle=":", which="both")

y = np.linspace(200000, 5000000, 400)
ax[0][1].plot(y,
              quantile(1 / y,
                       P=P,
                       c=popt[0],
                       loc=threshold,
                       scale=popt[1]),
              'r-', lw=1, color='blue', alpha=0.8)

ax[0][1].plot(y,
              quantile_conf_bound(1 / y,
                                  P=P,
                                  n=+1, 
                                  c=popt[0],
                                  loc=threshold,
                                  scale=popt[1],
                                  err_P=err_P,
                                  err_c= pcov[0][0]**0.5,
                                  err_scale=pcov[1][1]**0.5),
              'r-', lw=1, color='green', alpha=0.8)

ax[0][1].plot(y,
              quantile_conf_bound(1 / y,
                                  P=P,
                                  n=-1, 
                                  c=popt[0],
                                  loc=threshold,
                                  scale=popt[1],
                                  err_P=err_P,
                                  err_c= pcov[0][0]**0.5,
                                  err_scale=pcov[1][1]**0.5),
              'r-', lw=1, color='green', alpha=0.8)

props = dict(boxstyle='round', color='black', alpha=0.05)

ax[1][1].text(0.05, 0.85,
              fit_legend,
              fontsize=9,
              verticalalignment='top',
              bbox=props)

plt.savefig('figfit-all-loaded.png')
#plt.show()
