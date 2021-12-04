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

The [api](../../api) directory contains the public protocol buffer APIs of
Nighthawk and its components. See [the API](#the-apis) section for more details
on the exposed APIs.

The [benchmarks](../../benchmarks) directory contains a test suite built on top
of Nighthawk's integration tests that allows users to develop their own
benchmarks. See the [benchmarks documentation](../../benchmarks/README.md) for
more details.

The [ci](../../ci) directory contains configuration and scripts used when
executing continuous integration pipelines for Nighthawk.

The [docs](../../docs) contain Nighthawk's documentation.

The [extensions_build_config.bzl](../../extensions_build_config.bzl) selects the
extensions the Envoy used by Nighthawk is built with. See [Building Envoy with
Bazel](https://github.com/envoyproxy/envoy/blob/main/bazel/README.md) for
details. Nighthawk uses Envoy both in the client code to send requests and the
in the [Nighthawk test server](overview.md#nighthawk_test_server) to respond to
requests.

The [include](../../include) directory contains C++ header files (declarations)
of Nighthawk components. These aren't considered to be part of the public API
and may change as Nighthawk develops. See [the
include](#the-include-and-source-directories) section for more details about
this directory.

The [internal_proto](../../internal_proto) directory contains protocol buffer
messages that are internal to Nighthawk.

The [samples](../../samples) directory contains samples in the
[fortio](https://github.com/fortio/fortio#report-only-ui) data format. See the
[README.md](../../README.md#visualizing-the-output-of-a-benchmark) for more
details.

The [source](../../source) directory contains C++ implementations (definitions)
of Nighthawk components declared in the **include** directory as well as the
build targets for the Nighthawk binaries. See [the
source](#the-include-and-source-directories) section for more details about
this directory.

The [support](../../support) directory contains tools used when developing
Nighthawk, e.g. git pre-commit hooks. See its
[README.md](../../support/README.md) for more details.

The [test](../../test) contains Nighthawk's unit and integration tests. The
integration tests are located in the [integration](../../test/integration)
subdirectory.

The [tools](../../tools) directory contains utilities used to upkeep the
Nighthawk codebase, e.g. tools that enforce file formatting rules, etc.

The [WORKSPACE](../../WORKSPACE) file contains Bazel [workspace
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

The [adaptive_load](../../api/adaptive_load) directory contains protocol buffer
messages that are used to configure the [adaptive load
controller](adaptive_load_controller.md).

The [client](../../api/client) directory contains the main API of the Nighthawk
CLI and the gRPC service definition when Nighthawk runs as a gRPC server. See
[overview](overview.md) for more details.

The [distributor](../../api/distributor) and [sink](../../api/sink) directories
are related to an ongoing effort to allow Nighthawk to scale horizontally. See
[#369](https://github.com/envoyproxy/nighthawk/issues/369) for more details.

The [request_source](../../api/request_source) directory contains the APIs of
the request source when using its plugin or gRPC service implementation. See the
[overview](overview.md#requestsource) for a list of available request source
implementation and its role in the architecture of Nighthawk.

The [server](../../api/server) directory contains options that can be sent to
the [Nighthawk test server](overview.md#nighthawk_test_server).

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
The **adaptive_load** directories
([include](../../include/nighthawk/adaptive_load),
[source](../../source/adaptive_load)) contain the declarations and definitions
of components belonging to the [adaptive load
controller](adaptive_load_controller.md).

The **client** directories ([include](../../include/nighthawk/client),
[source](../../source/client)) contain the declarations and definitions of the
main Nighthawk components as outlined in the [overview](overview.md).

The **common** directories ([include](../../include/nighthawk/common),
[source](../../source/common)) contain code that is used by the Nighthawk
client and at least one other component, e.g. the Nighthawk test server, the
request source, etc.

The **distributor** ([include](../../include/nighthawk/distributor),
[source](../../source/distributor)) and **sink**
([include](../../include/nighthawk/sink), [source](../../source/sink))
directories are related to an ongoing effort to allow Nighthawk to scale
horizontally. See [#369](https://github.com/envoyproxy/nighthawk/issues/369) for
more details.

The [exe](../../source/exe) directory contains build targets for the main
[Nighthawk binaries](overview.md#nighthawk-binaries).

The **request_source** directories
([include](../../include/nighthawk/request_source),
[source](../../source/request_source)) contain the declarations and definitions
of components belonging to the implementations of the [request
source](overview.md#requestsource).

The [server](../../source/server) directory contains code belonging to the
[Nighthawk test server](overview.md#nighthawk_test_server).
