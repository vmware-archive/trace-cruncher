## [Release v0.4.0](https://github.com/vmware/trace-cruncher/compare/tracecruncher-v0.3.0...tracecruncher-v0.4.0)

> Release Date: 2023-06-26

 ### API Changes
- [40f3340b]	trace-cruncher: New API to get available dynamic events
- [edc58ef3]	trace-cruncher: New API to get available trace instances
- [71c5305e]	trace-cruncher: Improve synthetic events creation
- [03a87012]	trace-cruncher: Improve trace instance deletion

 ### Tests
- [c590c196]	trace-cruncher: Makefile target for unit tests

 ### Chore
- [3a4e9ebb]	trace-cruncher: Bump the versions of required trace libraries
- [02f9b380]	trace-cruncher: Fixed compilation warnings
- [461c340e]	trace-cruncher: Replace permission error with warning
- [882ab136]	trace-cruncher: Update dependences
- [4f028cf5]	tracetrace-cruncher: Update github workflow

## [Release v0.3.0](https://github.com/vmware/trace-cruncher/compare/tracecruncher-v0.2.0...tracecruncher-v0.3.0)

> Release Date: 2022-11-22

 ### Fix

- [582b32b0]	trace-cruncher: Fix waiting logic for external tasks
- [0f59976a]	trace-cruncher: Fix CI issue and add reproducibility tools
- [acd3b683]	setup.py: fix string <->int comparison

 ### API Changes

- [160a35f5]	trace-cruncher: New API for setting custom ftrace directory
- [046a51af]	trace-cruncher: Enhance events filter API
- [4a7ab528]	trace-cruncher: Add new ftraccepy API - wait()
- [17814de0]	trace-cruncher: High level wrappers for ftrace uprobes
- [be4f5b8a]	trace-cruncher: ftrace uprobe raw API
- [daa5c07e]	trace-cruncher: Logic for resolving address to function name

 ### Documentation

- [1de129cd]	trace-cruncher: Example script for uprobes high level API
- [a4563d6f]	trace-cruncher: Add	documentation for tracecruncher.ft_utils
- [737bed9a]	trace-cruncher: Add	documentation for tracecruncher.ftracepy

 ### Tests

- [b393c50b]	trace-cruncher: Integration test fix - do not assume 0 instances
- [49696616]	trace-cruncher: Link librt to unit test helper
- [c4ac99e9]	trace-cruncher: Improve unit test for ftracepy.wait API
- [d29be35c]	trace-cruncher: Unit test for ftracepy.wait API
- [b7fe2c2a]	trace-cruncher: Add new helper application for unit tests
- [a492bf8a]	trace-cruncher: Start testing branch 'tracecruncher'

 ### Chore

- [fa26505b]	trace-cruncher: Bump the versions of required trace libraries
- [6953af22]	trace-cruncher: Update README.md
- [0e3fa11f]	trace-cruncher: Updat git CI
- [5ccb241a]	trace-cruncher: Move helper functions into common librray
- [a8371b26]	Merge pull request #12 from knauth/tracecruncher
- [f89c6f99]	Merge remote-tracking branch 'knauth/fix-unit-tests' into tracecruncher
- [870c0866]	Merge pull request #7 from knauth/unit-tests-regressioncheck
- [dcd4d7db]	trace-cruncher: Use tcrunchbase library in ftracepy module
- [33d6a2c3]	trace-cruncher: Add	trace-cruncher CI manual trigger
- [ed52a622]	trace-cruncher: Introduce Docker installation
- [5e53786d]	Merge pull request #8 from knauth/tracecruncher
- [a03a625c]	trace-cruncher: Enable Github development flow
- [b43d5e90]	trace-cruncher: README updates; add Testing, reformat, and fix deps
- [6948cd45]	trace-cruncher: Add	object files to gitignore
- [5b8adef4]	trace-cruncher: Update github workflows
- [e7f7de28]	trace-cruncher: Add	define for bool type
- [78e3c4ac]	trace-cruncher: Build trace-obj-debug.c as library