# Maintainers

This document aims to assist [maintainers](OWNERS.md).

## Envoy domain expertise

As a guideline, concepts in Nighthawk that are derived from Envoy
require someone with Envoy domain expertise in review. Notable examples
are the way Nighthawk internal computes cluster configuration, its
connection pool derivations, the `StreamDecoder` class, as well as anything related to the Nighthawk test server.

See [OWNERS.md](OWNERS.md) to find maintainers with expertise of
Envoy internals.

## Pre-merge checklist

- Does the PR have breaking changes? Then that should be explicitly mentioned in the [version history](docs/root/version_history.md).
- New features should be added to the [version history](docs/root/version_history.md).
- Breaking changes to the [proto apis](api/) are not allowed.

## Updates to the Envoy dependency

We try to regularly synchronize our Envoy dependency with the latest revision. When we do so, that looks like:

- A change to [repositories.bzl](bazel/repositories.bzl) to update the commit and SHA.
- A sync of [.bazelrc](.bazelrc)
- A sync of the build image sha used in the [ci configuration](.circleci/config.yml).
- Any changes required to Nighthawk
