# Maintainers

This document aims to assist [maintainers](OWNERS.md).

## Envoy domain expertise

As a guideline, concepts in Nighthawk that are derived from Envoy require
someone with Envoy domain expertise in review. Notable examples are the way
Nighthawk internally computes cluster configuration, its connection pool
derivations, the `StreamDecoder` class, as well as anything related to the
Nighthawk test server.

See [OWNERS.md](OWNERS.md) to find maintainers with expertise of Envoy
internals.

## Pre-merge checklist

- Does the PR have breaking changes? Then that should be explicitly mentioned in
  the [version history](docs/root/version_history.md).
- New features should be added to the
  [version history](docs/root/version_history.md).
- Breaking changes to the [protobuf APIs](api/) are not allowed.
- When merging, clean up the commit message so we get a nice history. By
  default, github will compile a message from all the commits that are squashed.
  The PR title and description should be a good starting point for the final
  commit message. (If it is not, it may be worth asking the PR author to update
  the description).
- Make sure that the DCO signoff is included in the final commit message.
  - As a convention, it is appropriate to exclude content in the PR description
    that occurs after the signoff.

## Updates to the Envoy dependency

We aim to
[synchronize our Envoy dependency](https://github.com/envoyproxy/nighthawk/pulls?utf8=%E2%9C%93&q=is%3Apr+is%3Aclosed+%22update+envoy%22+)
with the latest revision weekly. Nighthawk reuses large parts of Envoy's build
system and codebase, so keeping Nighthawk up to date with Envoy's changes is an
important maintenance task.

See [Updating the Envoy Dependency](docs/root/updating_envoy_dependency.md) for
the procedure.

