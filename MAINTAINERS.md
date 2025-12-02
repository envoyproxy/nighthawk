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

The update process can be completed by manually executing the steps documented on this [page]( https://github.com/envoyproxy/nighthawk/blob/69321419308f36078037a05a6b12b9819368d413/docs/root/updating_envoy_dependency.md) **or** running a utility script. The rest of this page provides instructions for using the utility script.

### Run the utility script

The utility script will attempt to find the latest Envoy commit that
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
*   `--agent_invocation CMD`: Command to invoke an LLM agent (e.g., `gemini`) to
    attempt to automatically fix errors. The agent will be called with the
    prompt provided via stdin. Default: `None`

The script provides detailed output for each step it performs, prefixed with
`NighthawkEnvoyUpdate:` or `....EnvoyCommitIntegration:` for sub-steps.

The script is designed to be idempotent and can be re-run. If you encounter a
manual step (like resolving merge conflicts):

1.  Follow the instructions provided by the script.
2.  Commit your changes to the local Nighthawk branch.
3.  Re-run the *same command* you used to start the script.

The script will detect the existing branch, rebase it on `origin/main` if
necessary, and attempt to continue the update process.

### LLM-in-the-loop automated integration fix

To experimentaly enable automated fixes, you can use the `--agent_invocation`
argument. The utility will use the specified command (e.g. `gemini`) when a step
fails and pass a detailed prompt via stdin. The agent can then attempt to modify
the files in your local Nighthawk repository to resolve the issue. The utility
gives the agent 3 tries to fix an error before returning the error to the user.

```bash
./tools/nighthawk_envoy_updater.py \
  --nighthawk_dir ~/my_local_nighthawk_git_repo \
  --branch_name my-envoy-update-with-agent \
  --agent_invocation gemini
```

### Outcomes

*   **Initial checks fail:** A script prerequisite is violated, like the
    Nighthawk directory not existing, not being a git repo, or having
    uncommitted changes. The script assumes you've followed
    [Building Nighthawk](https://github.com/envoyproxy/nighthawk/blob/main/README.md#building-nighthawk)
    to set up your local Git repo.

    -   The script will exit early with an error message indicating which
        prerequisite is not satisfied.

    -   *Address the prerequisite and re-run the script with the same
        arguments.*

*   **Trivial update:** The latest Envoy commit integrates without any issues.

    -   The script will update files to the latest Envoy commit, commit those
        changes to your local Git repo, and push your update branch to your
        `origin` remote. It will print a link to create a pull request on
        GitHub.

    -   *You're done!*

*   **Non-trivial update:** The latest Envoy commit cannot be trivially
    integrated. The script will use bisection to identify the most recent Envoy
    commit that can be trivially integrated, and, its immediate successor, the
    oldest Envoy commit that *cannot* be trivially integrated.

    -   The script will update files to the most recent Envoy commit that can be
        trivially integrated, commit those changes to your local Git repo, and
        push your update branch to your `origin` remote. This brings your local
        Git repo branch with a commit that captures the last-known good state of
        the Nighthawk repo.

    -   The script applies as much of oldest Envoy commit that *cannot* be
        trivially integrated as it can.

    -   *Manually address all script prompts (resolve merge conflicts, address
        tooling failures, ensure tests pass, and commit these changes to the
        local Git repo) and then re-run the script with the same arguments.*

### General guidance to address Envoy commit integration failures

*   Look for `FAILED` in the update script output: it will print with an error
    message that provides context.
*   If `TEST_NIGHTHAWK` fails, you'll need to debug the test failures in the
    Nighthawk code. The logs from `./ci/do_ci.sh test` will be important.

By following the script's output and these guidelines, you can efficiently
update the Envoy dependency, handling both trivial and more complex updates.
