# Stress Test Configuration

This directory contains Bazel build settings for controlling test execution behavior.

## Problem Solved

Previously, the project used `action_env` to pass environment variables to tests, which causes:
- Complete rebuild/retest on any environment change
- No cache sharing between different users/CI systems
- Non-hermetic builds

## Solution

We now use Bazel's modern build setting mechanism to control stress test execution:

```bash
# Run with stress tests enabled
bazel test --//test/config:run_stress_tests=True //test/...

# Run without stress tests (default)
bazel test --//test/config:run_stress_tests=False //test/...
# or simply
bazel test //test/...
```

## How It Works

1. **Build Setting**: `//test/config:run_stress_tests` is a `bool_flag` build setting (default: False)

2. **Config Setting**: `//test/config:stress_tests_enabled` matches when the flag is set to True

3. **Test Environment**: The Python test binary uses `select()` to conditionally set environment variables based on the config_setting

4. **Test Detection**: Python tests check `os.environ.get("NH_RUN_STRESS_TESTS", "false") == "true"`

## Benefits

- **Modern**: Uses current Bazel best practices (build settings, not `--define`)
- **Hermetic**: Build configuration is explicit and reproducible
- **Cacheable**: Builds with the same flags share cache entries
- **Type-safe**: Boolean flags prevent typos and invalid values
- **Clear**: Test behavior is controlled by explicit command-line flags

## CI Usage

The CI automatically enables stress tests when running on branches:
- Pull requests and branch builds: `--//test/config:run_stress_tests=True`
- Local development (no GH_BRANCH): `--//test/config:run_stress_tests=False`

## Adding to .bazelrc

You can also create configurations in your `.bazelrc`:

```bash
# In .bazelrc or user.bazelrc
build:stress --//test/config:run_stress_tests=True

# Then use:
bazel test --config=stress //test/...
```
