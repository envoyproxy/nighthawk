# Release procedure

- Update [version_history.md](docs/root/version_history.md).
  - Make sure breaking changes are explicitly called out.
    Ideally these have been tracked on the go but it does not hurt to ask around and double
    check. Running the python integration tests of the previous release with the latest binaries could serve as a double check for this.
  - Ensure the release notes are complete. Ideally these have been tracked on the go.
- Perform some thorough testing to double down on stability.

```bash
bazel test --cache_test_results=no --test_env=ENVOY_IP_TEST_VERSIONS=all --runs_per_test=1000 --jobs 50 -c dbg --local_resources 20000,20,0.25 //test:*
```

- Draft a [GitHub tagged release](https://github.com/envoyproxy/nighthawk/releases/new) using the [current version number on master]((include/nighthawk/common/version.h)). Earlier releases are tagged like `v0.1` so currently we use that as a naming convention.
- [File an issue](https://github.com/envoyproxy/nighthawk/issues/new?title=[VOTE]+Release+v0.x&body=Release%20X%20is%20ready%20for%20review.%20Please%20take%20a%20look%20and%20vote!) to raise a vote on publishing the drafted release and point contributors to that.
- If nobody raises blocking issues, it's time to go ahead and publish the drafted release!
- Bump `MAJOR_VERSION` / `MINOR_VERSION` in [version.h](include/nighthawk/common/version.h) to the next version.
