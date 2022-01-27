

# Contributing to trace-cruncher

The trace-cruncher project team welcomes contributions from the community. All contributions to this repository must be
signed. Your signature certifies that you wrote the patch or have the right to pass it on as an open-source patch.

The development process of trace-cruncher is strongly coupled to the development of the Linux kernel tracing libraries [libtraceevent](https://git.kernel.org/pub/scm/libs/libtrace/libtraceevent.git), [libtracefs](https://git.kernel.org/pub/scm/libs/libtrace/libtracefs.git/) and [KernelShark](https://git.kernel.org/pub/scm/utils/trace-cmd/kernel-shark.git/). Because of this reason we adopt and follow the development workflow established by those projects.

For contributions to development, please send patches to: linux-trace-devel@vger.kernel.org

[Subscribe](http://vger.kernel.org/vger-lists.html#linux-trace-devel) / [Archives](https://lore.kernel.org/linux-trace-devel/)

### Code Style

The preferred coding style for the project is the [Linux kernel coding style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html#linux-kernel-coding-style)

### Formatting Commit Messages

The project follows the conventions for [submitting patches](https://www.kernel.org/doc/html/v5.4/process/submitting-patches.html)
as described by the Linux kernel.

### Tests

Make sure that all your changes are covered by the tests. Before submitting your patch, check if everything works at 100% by running the tests in **tracecruncher/tests**. Compile your changes and install trace-cruncher (`sudo make install`), to make sure that your code is used in the tests. As trace-cruncher interacts with the Linux kernel tracing infrastructure, the tests must be run with root privileges:

``` shell
cd tracecruncher/tests
sudo python3 -m unittest discover .
```

## Reporting Bugs and Creating Issues
For bug reports and issues, please file it [bugzilla](https://bugzilla.kernel.org/buglist.cgi?component=Trace-cmd%2FKernelshark&product=Tools&resolution=---)

When opening a new issue, try to roughly follow the commit message format conventions above.
