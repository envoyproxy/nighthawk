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

We try to [regularly synchronize our Envoy dependency](https://github.com/envoyproxy/nighthawk/pulls?utf8=%E2%9C%93&q=is%3Apr+is%3Aclosed+%22update+envoy%22+) with the latest revision. Nighthawk reuses large parts of Envoy's build system and CI infrastructure. When we update, that looks like:

- A change to [repositories.bzl](bazel/repositories.bzl) to update the commit and SHA.
- A sync of [.bazelrc](.bazelrc) with [Envoy's version](https://github.com/envoyproxy/envoy/blob/master/.bazelrc) to update our build configurations.
- A sync of the build image sha used in the [ci configuration](.circleci/config.yml) with [Envoy's version](https://github.com/envoyproxy/envoy/blob/master/.circleci/config.yml) to sync our CI testing environment.
- Sometimes the dependency update comes with changes that break our build. We include any changes required to Nighthawk to fix that.
