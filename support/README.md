# Support tools

A collection of CLI tools meant to support and automate various aspects of
developing Nighthawk, particularly those related to code review. For example,
automatic DCO signoff and pre-commit format checking.

## Usage

To get started, you need only navigate to the Nighthawk project root and run:

```bash
./support/bootstrap
```

This will set up the development support toolchain automatically. The toolchain
uses git hooks extensively, copying them from `support/hooks` to the `.git`
folder.

The commit hook checks can be skipped using the `-n` / `--no-verify` flags, as
so:

```bash
git commit --no-verify
```

## Functionality

Currently the development support toolchain exposes two main pieces of
functionality:

- Automatically appending DCO signoff to the end of a commit message if it
  doesn't exist yet.
- Automatically running DCO and format checks on all files in the diff, before
  push.

## Fixing Format Problems

If the pre-push format checks detect any problems, you can either fix the
affected files manually or run the provided formatting script.

To run the format fix script directly:

```
ci/do_ci.sh fix_format
```
