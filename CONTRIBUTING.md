

# Contributing to trace-cruncher

The trace-cruncher project team welcomes contributions from the community. All contributions to this repository must be
signed. Your signature certifies that you wrote the patch or have the right to pass it on as an open-source patch.

### Submitting patches

Patches can be submitted by either:
 1. Using the regular [Github Flow](https://docs.github.com/en/get-started/quickstart/github-flow):
    - The main branch is **tracecruncher**.
    - Any changes must be on a feature branch or on a fork.
    - Tests must pass before merging, and the pull request must be [reviewed](https://docs.github.com/en/github/collaborating-with-pull-requests/reviewing-changes-in-pull-requests) and approved.
    - Break the complex Pull Requests into small self-contained patches.

 2. Using the [Linux kernel](https://www.kernel.org/doc/html/v5.4/process/submitting-patches.html) development workflow:
    - Add prefix "trace-cruncher:" to the patch subject.
    - Break the changes into small self-contained patches and group them in a patch set.
    - Send patches to linux-trace-devel@vger.kernel.org
    - [Subscribe](http://vger.kernel.org/vger-lists.html#linux-trace-devel) / [Archives](https://lore.kernel.org/linux-trace-devel/)

### Code Style

The preferred coding style for the project is the [Linux kernel coding style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html#linux-kernel-coding-style)

### Tests

Make sure that all your changes are covered by the tests. Before submitting your patch, check if everything works at 100% by running the tests in **tracecruncher/tests**. Compile your changes and install trace-cruncher (`sudo make install`), to make sure that your code is used in the tests. As trace-cruncher interacts with the Linux kernel tracing infrastructure, the tests must be run with root privileges:

``` shell
cd tests
sudo python3 -m unittest discover .
```

## Reporting Bugs and Creating Issues
Bug reports and issues can be submitted by either:
1. Create a [Github issue](https://docs.github.com/es/issues/tracking-your-work-with-issues/creating-an-issue) in [trace-cruncher](https://github.com/vmware/trace-cruncher).
2. Report them in [bugzilla](https://bugzilla.kernel.org/buglist.cgi?component=Trace-cmd%2FKernelshark&product=Tools&resolution=---), **Tools and utilities** category, **Trace-cmd and kernelshark** component.
