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

-   Does the PR have breaking changes? Then that should be explicitly mentioned
    in the [version history](docs/root/version_history.md).
-   New features should be added to the
    [version history](docs/root/version_history.md).
-   Breaking changes to the [protobuf APIs](api/) are not allowed.
-   When merging, clean up the commit message so we get a nice history. By
    default, github will compile a message from all the commits that are
    squashed. The PR title and description should be a good starting point for
    the final commit message. (If it is not, it may be worth asking the PR
    author to update the description).
-   Make sure that the DCO signoff is included in the final commit message.
    -   As a convention, it is appropriate to exclude content in the PR
        description that occurs after the signoff.

## Updates to the Envoy dependency

We aim to
[synchronize our Envoy dependency](https://github.com/envoyproxy/nighthawk/pulls?utf8=%E2%9C%93&q=is%3Apr+is%3Aclosed+%22update+envoy%22+)
with the latest revision weekly. Nighthawk reuses large parts of Envoy's build
system and codebase, so keeping Nighthawk up to date with Envoy's changes is an
important maintenance task.

We use a utility script that will attempt to find the latest Envoy commit that
integrates cleanly with Nighthawk and create a Github branch with that
integration ready to merge. If it encounters a non-trivial Envoy commit
integration, it will prompt the user to resolve any errors before proceeding.

```bash
./tools/nighthawk_envoy_updater.py \
  --nighthawk_dir ~/my_local_nighthawk_git_repo \
  --branch_name my-envoy-update
```

You can customize the script's behavior using the following arguments:

*   `--nighthawk_dir PATH`: Specifies the path to your local Nighthawk git
    repository clone. Default: `~/github/nighthawk`
*   `--envoy_clone_depth INT`: Sets the depth for cloning the Envoy repository.
    Increase this if the script can't find the current Envoy commit used in
    Nighthawk. Default: `200`
*   `--branch_name NAME`: The name of the branch to create in the Nighthawk
    repository for the update. Default: `update-envoy-YYYYMMDD` (e.g.,
    `update-envoy-20250809`)

The script provides detailed output for each step it performs, prefixed with
`NighthawkEnvoyUpdate:` or `....EnvoyCommitIntegration:` for sub-steps.

### Success Scenarios

*   **Fully trivial update:** If the latest Envoy commit integrates without any
    issues, the script will update the branch and push it to your `origin`
    remote. It will print a link to create a pull request on GitHub.
*   **Partial trivial update:** If the latest Envoy commit fails, but an older
    commit between the current and latest integrates cleanly, the script will
    use that older commit. It will push the branch and provide a PR link.

### Failure Scenarios

*   **Initial checks fail:** If prerequisites like the Nighthawk directory not
    existing, not being a git repo, or having uncommitted changes, the script
    will exit early with an error message.
*   **Bisection finds a non-trivial merge:** The script updates your local
    Nighthawk git repo to the latest trivial integration, if any (this is the
    "partial trivial update" success scenario). It then applies as much of the
    *next* commit after the last good one as possible before prompting the user
    to resolve merge conflicts and address failing tests before proceeding. You
    should:
    1.  Resolve merge conflicts.
    2.  Manually run tests and make code modifications until they pass:
        `./ci/do_ci.sh test`
    3.  **Commit** your new fixes.
    4.  Re-run the updater script with the same arguments. It will proceed with
        the bisection from the commit you just applied.

**General Guidance on Failures:**

*   Look for `FAILED` in the output: it will print with an error message that
    provides context.
*   If `PATCH_SHARED_FILES` within `EnvoyCommitIntegration` fails, check for
    `.rej` files in your Nighthawk directory and manually resolve the conflicts.
*   If `TEST_NIGHTHAWK` fails, you'll need to debug the test failures in the
    Nighthawk code. The logs from `./ci/do_ci.sh test` will be important.

By following the script's output and these guidelines, you can efficiently
update the Envoy dependency, handling both trivial and more complex updates.
