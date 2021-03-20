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
important maintenance task. When performing the update, follow this procedure:

1. Create a fork of Nighthawk, or fetch upstream and merge changes into your
   fork if you already have one.
1. Create a new branch from `main`, e.g. `envoy-update`.
1. Edit [bazel/repositories.bzl](bazel/repositories.bzl).
   1. Update `ENVOY_COMMIT` to the latest Envoy's commit from 
      [this page](https://github.com/envoyproxy/envoy/commits/main). (Clicking on the
      short commit id opens a page that contains the fully expanded commit id).
   1. Set `ENVOY_SHA` to an empty string initially, we will get the correct
      sha256 after the first bazel execution.
   Example content of `bazel/repositories.bzl` after the edits:
   ```
   ENVOY_COMMIT = "9753819331d1547c4b8294546a6461a3777958f5"  # Jan 24th, 2021
   ENVOY_SHA = ""
   ```
  1. Run `ci/do_ci.sh build`, notice the sha256 value at the top of the output,
     example:
  ```
  INFO: SHA256 (https://github.com/envoyproxy/envoy/archive/9753819331d1547c4b8294546a6461a3777958f5.tar.gz) = f4d26c7e78c0a478d959ea8bc877f260d4658a8b44e294e3a400f20ad44d41a3
  ```
1. Update `ENVOY_SHA` in [bazel/repositories.bzl](bazel/repositories.bzl) to
     this value.
1. Sync (copy) [.bazelrc](.bazelrc) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/.bazelrc) to
   update our build configurations. Be sure to retain our local modifications,
   all lines that are unique to Nighthawk are marked with comment `# unique`.
1. In the updated [.bazelrc](.bazelrc) search for `experimental_docker_image`.
   Copy the SHA and update `envoyproxy/envoy-build-ubuntu` over at the top of [.circleci/config.yml](.circleci/config.yml).
1. Sync (copy) [.bazelversion](.bazelversion) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/.bazelversion)
   to ensure we are using the same build system version.
1. Sync (copy) [ci/run_envoy_docker.sh](ci/run_envoy_docker.sh) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/ci/run_envoy_docker.sh).
1. Run `ci/do_ci.sh test`. Sometimes the dependency update comes with changes
   that break our build. Include any changes required to Nighthawk to fix that
   in the same PR.
1. Create a PR with a title like `Update Envoy to 9753819 (Jan 24th 2021)`,
   describe all performed changes in the PR's descriotion.
1. If the PR ends up modifying any c++ files, execute `ci/do_ci.sh fix_format`
   to reformat the files and avoid a CI failure.
1. If the PR ends up modifying any CLI arguments, execute
   `tools/update_cli_readme_documentation.sh --mode fix` to regenerate the
   portion of our documentation that captures the CLI help output. This will
   prevent a CI failure.
