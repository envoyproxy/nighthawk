# Navigating the Nighthawk codebase

This document outlines how to navigate the Nighthawk codebase. The top level
directory structure of Nighthawk's repository looks as follows:

```
.
├── api
├── benchmarks
├── ci
├── docs
├── extensions_build_config.bzl
├── include
├── internal_proto
├── samples
├── source
├── support
├── test
├── tools
└── WORKSPACE
```

The **api** directory contains the public APIs of Nighthawk and its components.
See [the API](#the-apis) section for more details on the exposed APIs.

The **benchmarks** directory contains a test suite built on top of Nighthawk's
integration tests that allows users to develop their own benchmarks. See the
[benchmarks documentation](../../benchmarks/README.md) for more details.

The **ci** directory contains configuration and scripts used when executing
continuous integration pipelines for Nighthawk.

The **docs** contain Nighthawk's documentation.

The **extensions_build_config.bzl** selects the extensions the Envoy used by
Nighthawk is built with. See [Building Envoy with
Bazel](https://github.com/envoyproxy/envoy/blob/main/bazel/README.md) for
details.

The **include** directory contains C++ header files of Nighthawk components.
These aren't considered to be part of the public API and may change as
Nighthawk  develops. See [the include](#the-include-and-source-directories)
section for more details about this directory.

The **internal_proto** directory contains protocol buffer message definitions
that are internal to Nighthawk.

The **samples** directory contains samples in the
[fortio](https://github.com/fortio/fortio#report-only-ui) data format. See the
[README.md](../../README.md#visualizing-the-output-of-a-benchmark) for more
details.

The **source** directory contains C++ implementations (definitions) of
nighthawk components declared in the **include** directory as well as the build
targets for the Nighthawk binaries. See [the
source](#the-include-and-source-directories) section for more details about
this directory.

The **support** directory contains tools used when developing Nighthawk, e.g.
git pre-commit hooks. See its [README.md](../../support/README.md) for more
details.

The **test** contains Nighthawk's unit and integration tests. The integration
tests are located in the **integration** subdirectory.

The **tools** directory contains utilities used to upkeep the Nighthawk
codebase, e.g. tools that enforce file formatting rules, etc.

The **WORKSPACE** file contains Bazel [workspace
rules](https://docs.bazel.build/versions/main/be/workspace.html) that define
dependencies external to this repository.

## The APIs

The **api** directory has the following subdirectories:

```
.
└── api
    ├── adaptive_load
    ├── client
    ├── distributor
    ├── request_source
    ├── server
    └── sink
```

The **adaptive_load** directory contains protocol buffer messages that are used
to configure the [adaptive load controller](adaptive_load_controller.md).

The **client** directory contains the main API of the Nighthawk CLI and the
gRPC service definition when Nighthawk runs as a gRPC server. See
[overview](overview.md) for more details.

The **distributor** and **sink** directories are related to an ongoing effort
to allow Nighthawk to scale horizontally. See
[#369](https://github.com/envoyproxy/nighthawk/issues/369) for more details.

The **request_source** directory contains the APIs of the request source when
using its plugin or gRPC service implementation. See the
[overview](overview.md#requestsource) for a list of available request source
implementation and its role in the architecture of Nighthawk.

The **server** directory contains options that can be sent to the [Nighthawk
test server](overview.md#nighthawk_test_server).

## The include and source directories

The **include** directory has the following subdirectories:

```
.
└── include
    └── nighthawk
        ├── adaptive_load
        ├── client
        ├── common
        ├── distributor
        ├── request_source
        └── sink
```

The **source** directory has the following subdirectories:

```
.
└── source
    ├── adaptive_load
    ├── client
    ├── common
    ├── distributor
    ├── exe
    ├── request_source
    ├── server
    └── sink
```
The **adaptive_load** directories contain the declarations and definitions of
components belonging to the [adaptive load
controller](adaptive_load_controller.md).

The **client** directories contain declarations and definitions of the main
Nighthawk components as outlined in the [overview](overview.md).

The **common** directories contain code that is used by the Nighthawk client
and at least one other component, e.g. the Nighthawk test server, the request
source, etc.

The **distributor** and **sink** directories are related to an ongoing effort
to allow Nighthawk to scale horizontally. See
[#369](https://github.com/envoyproxy/nighthawk/issues/369) for more details.

The **exe** directory contains build targets for the main [Nighthawk
binaries](overview.md#nighthawk-binaries).

The **request_source** directories contain the declarations and definitions of
components belonging to the implementations of the [request
source](overview.md#requestsource).

The **server** directory contains the declarations and definitions belonging to
the [Nighthawk test server](overview.md#nighthawk_test_server).
