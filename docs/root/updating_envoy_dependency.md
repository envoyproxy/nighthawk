# Updating Nighthawk's Envoy Dependency

This document aims to assist [maintainers](/OWNERS.md).

For general information about maintainer responsibilities in the Nighthawk codebase, see [MAINTAINERS.md](/MAINTAINERS.md).

## Background

We aim to synchronize our Envoy dependency with the latest revision **weekly**
([PRs](https://github.com/envoyproxy/nighthawk/pulls?utf8=%E2%9C%93&q=is%3Apr+is%3Aclosed+%22update+envoy%22+)).

Nighthawk reuses large parts of Envoy's build
system and codebase, so keeping Nighthawk up to date with Envoy's changes is an
important maintenance task.

## Update Procedure

The text of each step is **official**, but the example shell commands are **suggestions**.
Feel free to accomplish each step in any way you prefer, and please update the
text or commands if you notice any issues.

It is highly recommended to perform the shell commands in a `screen` or `tmux`
session to avoid losing the shell variables that are accumulated across commands.

All example commands in this document are **ready to paste**.

### Step 0

Ensure you are in the directory expected by the steps in this guide:

```bash
ls -d .git api ci docs include source || (echo "These steps should be executed in a directory containing .git, api, ci, docs, include, source, etc.")
```

Ensure you have replaced Bazel with Bazelisk:

```bash
file `which bazel`
```

Example output: `/usr/bin/bazel: symbolic link to /home/xyz/go/bin/bazelisk`

### Step 1

Create a fork of Nighthawk, or fetch upstream and merge changes into your fork if you already have one.

#### Example commands

The following commands assume that you already have a fork and that the remote of `https://github.com/envoyproxy/nighthawk` is named `upstream`.

```bash
git fetch
git fetch upstream
git checkout main
git merge upstream/main
git push
```

### Step 2

Create a new branch from `main`, e.g. `envoy-update-123456789`.

#### Example commands

```bash
git checkout main
branch="envoy-update-$(date +%s)"
git checkout -b $branch
```

### Step 3

Clone the Envoy repo (https://github.com/envoyproxy/envoy.git) into a temp directory.


#### Example commands

```bash
envoy_clone_dir=$(mktemp -d -t envoy-XXXXXXXXXX)
pushd $envoy_clone_dir
git clone https://github.com/envoyproxy/envoy.git
popd

envoy_dir="$envoy_clone_dir/envoy"

pushd $envoy_dir
envoy_commit=$(git log --pretty=%H | head -1)
popd

echo "envoy_clone_dir=$envoy_clone_dir"
echo "envoy_dir=$envoy_dir"
echo "envoy_commit=$envoy_commit"
echo "Click here: https://github.com/envoyproxy/envoy/commit/$envoy_commit"
```

Click the link in the terminal to double check the date of the Envoy commit to which we will be updating Nighthawk.

### Step 4

(See **Example commands** for shell commands covering this entire step.)

Edit [bazel/repositories.bzl](/bazel/repositories.bzl):
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
1. Update `ENVOY_SHA` in [bazel/repositories.bzl](/bazel/repositories.bzl) to
      this value.

#### Example commands

Overwrite `ENVOY_COMMIT` and `ENVOY_SHA` in `bazel/repositories.bzl`:

```bash
sed -i -e "s/ENVOY_COMMIT =.*/ENVOY_COMMIT = \"${envoy_commit}\"/" bazel/repositories.bzl

sed -i -e "s/ENVOY_SHA =.*/ENVOY_SHA = \"\"/" bazel/repositories.bzl

git diff
```

At this point:

- `ENVOY_COMMIT` should be a new value
- `ENVOY_SHA` should be blank

The easiest way to obtain the Envoy SHA is to run a build from a clean state. In
this case the Envoy SHA is printed to Bazel's stdout near the beginning of the
build. The following commands extract the Envoy SHA from the Bazel's stdout (assuming the build progresses far
enough):


```bash
bazel clean --expunge

envoy_sha=$(ci/do_ci.sh build 2>&1 | head -30 | egrep -m 1 -o '[0-9a-f]{64}')

echo "envoy_sha=$envoy_sha"
```

Note that this will wait for the whole build to finish.

If this command failed to set `$envoy_sha` to a long alphanumeric string, most likely the build
failed because of code changes upstream. We will need to run `ci/do_ci.sh build` to debug at this point.

If we successfully obtained the new SHA, set `ENVOY_SHA`:

```bash
sed -i -e "s/ENVOY_SHA =.*/ENVOY_SHA = \"$envoy_sha\"/" bazel/repositories.bzl

git diff
```

At this point:

- `ENVOY_COMMIT` should be a new value
- `ENVOY_SHA` should be a new value

### Step 5

Set up a Bash function `merge_from_envoy` that will be used repeatedly in the example commands in steps 6-10.

Paste the following into the shell:

```bash
merge_from_envoy() {
  # $1 = relative path to Nighthawk file derived from a version in the Envoy repo
  relpath=$1

  pushd $envoy_dir
  git log $relpath | head
  popd

  diff $envoy_dir/$relpath $relpath || true

  echo "Determine if $relpath needs to be manually merged:"
  echo "  Any differences not marked with '# unique'?"
  echo "  If so, edit $relpath in another terminal before continuing."
}
```

This will only work in a shell where `$envoy_dir` was previously set and we
cloned Envoy locally (see Step 3).

When running this function, in the terminal you will see:

- commit id of Envoy's version of the file (e.g. c9d883afbd9bf5046f6bb6dbfab724bbcc104123)
- a diff of Envoy (left) and Nighthawk (right)

Our goal is to transfer changes from Envoy to Nighthawk, preserving certain Nighthawk-specific changes marked with `#unique`.

To take a closer look at an Envoy commit, visit `https://github.com/envoyproxy/envoy/commit/INSERT_COMMIT_ID_HERE`. This is helpful when it's hard to tell from the diff how the file should look.

Updates in the Envoy file should be selectively pasted into the Nighthawk file using a text editor, possibly in a second terminal.

Once you have done some partial work, save the file and repeat the `merge_from_envoy` command in the original terminal to check the diff again. This can be done iteratively.

### Step 6

Sync (copy) [.bazelrc](/.bazelrc) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/.bazelrc) to
   update our build configurations. Be sure to retain our local modifications,
   all lines that are unique to Nighthawk are marked with comment `# unique`.

#### Example commands

```bash
merge_from_envoy ".bazelrc"
```

### Step 7

Sync (copy) [.bazelversion](/.bazelversion) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/.bazelversion)
   to ensure we are using the same build system version.

#### Example commands

```bash
cp -v "$envoy_dir/.bazelversion" ".bazelversion"
```

### Step 8

Sync (copy) [ci/run_envoy_docker.sh](/ci/run_envoy_docker.sh) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/ci/run_envoy_docker.sh).
   Be sure to retain our local modifications, all lines that are unique to
   Nighthawk are marked with comment `# unique`.

#### Example commands

```bash
merge_from_envoy "ci/run_envoy_docker.sh"
```

### Step 9

Sync (copy) [tools/gen_compilation_database.py](/tools/gen_compilation_database.py) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/tools/gen_compilation_database.py) to
   update our build configurations. Be sure to retain our local modifications,
   all lines that are unique to Nighthawk are marked with comment `# unique`.

#### Example commands

```bash
merge_from_envoy "tools/gen_compilation_database.py"
```

### Step 10

Sync (copy) [tools/code_format/config.yaml](/tools/code_format/config.yaml) from
   [Envoy's version](https://github.com/envoyproxy/envoy/blob/main/tools/code_format/config.yaml) to
   update our format checker configuration. Be sure to retain our local modifications,
   all lines that are unique to Nighthawk are marked with comment `# unique`.

#### Example commands

```bash
merge_from_envoy "tools/code_format/config.yaml"
```

### Step 11
Perform this step if [tools/base/requirements.in](/tools/base/requirements.in)
has not been updated in the last 30 days (based on comment at top of file).

```bash
head -1 tools/base/requirements.in
```

- If less than 30 days ago, skip to next step.
- If more than 30 days ago, do the rest of this step.

The Python dependencies need to be updated regularly. The list of packages
the Nighthawk codebase uses is listed in
[tools/base/requirements.in](/tools/base/requirements.in). This file specifies
version constraints and it is our goal to use the latest but still compatible
version of every package. Ideally all constraints are in the `>=` format. If an
incompatibility is found, you can pin a package by specifying a `<=` constraint.
These should always be accompanied with a comment explaining them. Avoid using
`==` constraint to the extent possible.

First attempt to remove all existing pins in
[tools/base/requirements.in](/tools/base/requirements.in) to see if they are
still necessary. Once done editing
[tools/base/requirements.in](/tools/base/requirements.in), delete the contents
of [tools/base/requirements.txt](/tools/base/requirements.txt) and update the
dependencies by running:

```bash
echo > tools/base/requirements.txt
bazel run //tools/base:requirements.update
```

This will use the configuration from
[tools/base/requirements.in](/tools/base/requirements.in) and update the lock
file [tools/base/requirements.txt](/tools/base/requirements.txt).

### Step 12

Run:

```bash
ci/do_ci.sh build
```

Sometimes the dependency update comes with changes
   that break our build. Include any changes required to Nighthawk to fix that
   in the same PR.

If there are build failures, you will need to fix them at this point.

Then ensure that unit and integration tests pass:

```bash
ci/do_ci.sh test
```

Some test failures require code changes to fix.

See [Troubleshooting](#troubleshooting) for tips.

If you removed any pins or updated Python dependencies in the previous step, you
may see new failures due to these updates. Re-introduce dependency pins as necessary and execute the update
command Step 11 again. Repeat this until the tests pass and document the need for any
pins in [tools/base/requirements.in](/tools/base/requirements.in).

If you updated the Python dependencies, update the date at the top of the
[tools/base/requirements.in](/tools/base/requirements.in) file.

### Step 13

If the PR ends up modifying any C++ files, execute:

```bash
ci/do_ci.sh fix_format
```

to reformat the files and avoid a CI format check failure.


If you get a Python error message not related to the purpose of the script, this can sometimes be fixed by:

```bash
rm -rf tools/pyformat/
```

and retrying the format command.


### Step 14

If Nighthawk command line flags have been changed, execute:

```bash
tools/update_cli_readme_documentation.sh --mode fix
```

to regenerate the
   portion of our documentation that captures the CLI help output. This will
   prevent a CI failure in case any flags changed in the PR or upstream.

### Step 15

Create a PR with a title like `Update Envoy to 9753819 (Jan 24th 2021)`,
   describe all performed changes in the PR's description ([example PR
   description](https://github.com/envoyproxy/nighthawk/pull/758)).

#### Example commands

Check how we are doing:

```bash
git diff
```

Create the commit locally:

```bash
git add .
git commit -m "Update Envoy to $(echo $envoy_commit | cut -c 1-7) ($(date +'%b %d, %Y'))"
```

Upload the commit to your fork on GitHub:

```bash
git push --set-upstream origin $branch
```

Now go to https://github.com/envoyproxy/nighthawk and create a draft PR.

When the CI is passing, convert it to a regular PR and send it for review.

## Cleanup

```bash
rm -rf $envoy_clone_dir

git checkout main
```

## Troubleshooting

### Bazel Python Error

If you encounter an error that looks like:

```
ERROR: REDACTED/nighthawk/test/integration/BUILD:32:11: no such package '@nh_pip3//pypi__more_itertools':
BUILD file not found in directory 'pypi__more_itertools' of external repository @nh_pip3. Add a BUILD
file to a directory to mark it as a package. and referenced by '//test/integration:integration_test_base_lean'
```

Then we are missing a dependency from `requirements.txt`. This may happen due to changing other
dependencies.

The name of the dependency to add is everything after `pypi__`, in the above case `more_itertools`.

### Identifying an Envoy commit that introduced a breakage

#### Background

Sometimes CI tests fail after updating Nighthawk to an Envoy dependency. If the
root cause is hard to be identified, the bisect could be worth a try.

#### How to bisect

The bisect is used to find the problematic Envoy commit between the commit from
the last successful Nighthawk update and the current commit that Nighthawk needs
to be updated to.

Following the
[update process](/MAINTAINERS.md#updates-to-the-envoy-dependency) to update Nighthawk to a
specific Envoy commit for each Envoy commit being tested in the bisect. Generally, you can
do this by only changing the `ENVOY_COMMIT` and leaving `ENVOY_SHA` blank in the
`repositories.bzl` file.

- Usually the local `do_ci.sh` test is enough and the most efficient.
- If you do need to test in github CI (e.g. the local `do_ci.sh` passes while the
  github CI fails), creating a draft PR to execute the CI tests. See an example PR
  for bisecting [here](https://github.com/envoyproxy/nighthawk/pull/874).

#### Optimizations to speed up testing

- You can just test the failed tests by commenting out the others. For testing
  locally, modifying `ci/do_ci.sh`. For testing in the github CI, modifying
  `.azure-pipelines/pipelines.yml` (See this [example](https://github.com/envoyproxy/nighthawk/pull/874/files)).

- If it is the unit test or integration test that fails, you can modify the
  test code to only run the failure tests. See `test/python_test.cc` in this
  [PR](https://github.com/envoyproxy/nighthawk/pull/874/files) for running the selected Nighthawk python integration tests.
