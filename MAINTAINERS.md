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
1. Edit [bazel/repositories.bzl](bazel/repositories.bzl)
   1. Update `ENVOY_COMMIT` to the latest Envoy's commit from 
      [this page](https://github.com/envoyproxy/envoy/commits/main). (Clicking on the
      short commit id opens a page that contains the fully expanded commit id).
   1. Set `ENVOY_SHA` to an empty string initially, we will get the correct
      sha256 after the first bazel execution.
      Example content of `bazel/repositories.bzl` after the edits:
         ```
         ENVOY_COMMIT = "9753819331d1547c4b8294546a6461a3777958f5"
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
1. Sync (copy) [.bazelversion](.bazelversion) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/.bazelversion)
   to ensure we are using the same build system version.
1. Sync (copy) [ci/run_envoy_docker.sh](ci/run_envoy_docker.sh) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/ci/run_envoy_docker.sh).
   Be sure to retain our local modifications, all lines that are unique to
   Nighthawk are marked with comment `# unique`.
1. Sync (copy) [tools/gen_compilation_database.py](tools/gen_compilation_database.py) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/tools/gen_compilation_database.py) to
   update our build configurations. Be sure to retain our local modifications,
   all lines that are unique to Nighthawk are marked with comment `# unique`.
1. If [requirements.txt](requirements.txt) has not been updated in the last month (based on comment at top
   of file), check for major dependency updates. See
   [Finding python dependencies](#finding-python-dependencies) below for instructions.
1. Run `ci/do_ci.sh test`. Sometimes the dependency update comes with changes
   that break our build. Include any changes required to Nighthawk to fix that
   in the same PR.
1. If the PR ends up modifying any c++ files, execute `ci/do_ci.sh fix_format`
   to reformat the files and avoid a CI failure.
1. Execute `tools/update_cli_readme_documentation.sh --mode fix` to regenerate the
   portion of our documentation that captures the CLI help output. This will
   prevent a CI failure in case any flags changed in the PR or upstream.
1. Create a PR with a title like `Update Envoy to 9753819 (Jan 24th 2021)`,
   describe all performed changes in the PR's description.

## Finding python dependencies

We should check our python dependencies periodically for major version updates. We attempt to
update these dependencies monthly. Here is an easy way to check for major dependency updates:

1. Create and activate a virtual env:
   ```
   virtualenv pip_update_env
   source pip_update_env/bin/activate
   ```
   NOTE: if `pip_update_env/bin/activate` appears to not exist, try
   `pip_update_env/local/bin/activate` instead.

1. Install dependencies:

   ```
   pip install -r requirements.txt
   ```

1. Check for outdated dependencies:

   ```
   pip list --outdated
   ```
   This will likely show both outdated dependencies based on requirements.txt and other outdated
   dependencies you may have in addition, such as to `pip` itself. Here, we are only interested in
   cross-referencing the ones that appear with the ones in requirements.txt.

1. If you find any dependency updates, you can either try updating the dependency in requirements.txt yourself
   or create an issue for the change and assign it to one of the nighthawk maintainers.

   If there are not any dependency updates, please update the timestamp at the top of the file.

1. When done, clean up the virtual env:

   ```
   deactivate
   rm -rf pip_update_env
   ```

### Bazel Python Error

If you encounter an error that looks like:

```
RROR: REDACTED/nighthawk/test/integration/BUILD:32:11: no such package '@python_pip_deps//pypi__more_itertools':
BUILD file not found in directory 'pypi__more_itertools' of external repository @python_pip_deps. Add a BUILD
file to a directory to mark it as a package. and referenced by '//test/integration:integration_test_base_lean'
```

Then we are missing a dependency from requirements.txt. This may happen due to changing other
dependencies.

The name of the dependency to add is everything after `pypi__`, in the above case `more_itertools`.

## Identifying an Envoy commit that introduced a breakage

### Background

Sometimes CI tests fail after updating Nighthawk to an Envoy dependency. If the
root cause is hard to be identified, the bisect could be worth a try.

### How to bisect

The bisect is used to find the problematic Envoy commit between the commit from
the last successful Nighthawk update and the current commit that Nighthawk needs
to be updated to.

Following the
[update process](MAINTAINERS.md#updates-to-the-envoy-dependency) to update Nighthawk to a
specific Envoy commit for each Envoy commit being tested in the bisect. Generally, you can
do this by only changing the `ENVOY_COMMIT` and leaving `ENVOY_SHA` blank in the
`repositories.bzl` file.

- Usually the local `do_ci.sh` test is enough and the most efficient.
- If you do need to test in github CI (e.g. the local `do_ci.sh` passes while the
  github CI fails), creating a draft PR to execute the CI tests. See an example PR
  for bisecting [here](https://github.com/envoyproxy/nighthawk/pull/874).

### Optimizations to speed up testing

- You can just test the failed tests by commenting out the others. For testing
  locally, modifying `ci/do_ci.sh`. For testing in the github CI, modifying
  `.azure-pipelines/pipelines.yml` (See this [example](https://github.com/envoyproxy/nighthawk/pull/874/files)).

- If it is the unit test or integration test that fails, you can modify the
  test code to only run the failure tests. See `test/python_test.cc` in this
  [PR](https://github.com/envoyproxy/nighthawk/pull/874/files) for running the selected Nighthawk python integration tests.
