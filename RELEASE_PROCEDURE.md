# Release procedure

## Preparation

- Consolidate the [release notes](docs/root/version_history.md).
  - Make sure breaking changes are explicitly called out.
    Ideally these have been tracked on the go but it does not hurt to ask around and double
    check. Running the python integration tests of the previous release with the latest binaries could serve as a double check for this.
  - Ensure the release notes are complete. Ideally these have been tracked on the go.
- Based on our [semantic versioning](https://semver.org/spec/v2.0.0.html) strategy, our release numbers look like `MAJOR.MINOR.PATCH`. Determine the release type to come up with what the next version should look like:
  - Breaking changes increment `MAJOR`, and reset `MINOR.PATCH` to `0.0`
  - Added functionality which is backwards compatible increments `MINOR`, and resets `PATCH` to `0`
  - Bug fixes increment `PATCH`

## Release steps

1. Speculatively bump the version in [version_info.h](source/common/version_info.h) to the version you determined earlier. This may result in version gaps if a release attempt fails, but avoids having to freeze merges to main and/or having to work with release branches. In short it helps keeping the release procedure lean and mean and eliminates the need for blocking others while this procedure is in-flight.
2. Draft a [GitHub tagged release](https://github.com/envoyproxy/nighthawk/releases/new). Earlier releases are tagged like `v0.1`, but as of `0.3.0`we are using [semantic versioning](https://semver.org/spec/v2.0.0.html)
3. Perform thorough testing of the targeted revision to double down on stability [1]
4. Create an optimized build for comparing with the previous release. Changes in performance
  and/or measurement accuracy may need to be considered breaking. If in doubt, discuss!
  TODO(#245): Formalize and/or automate this.
5. If things look good, [File an issue](https://github.com/envoyproxy/nighthawk/issues/new?title=%5BVOTE%5D+Release+v0.x.x&body=Release+v0.x.x%20is%20ready%20for%20review.%20Please%20take%20a%20look%20and%20vote!!) to raise a vote on publishing the drafted release, and point contributors to that.
6. Allow sufficient time to transpire for others to provide feedback, before moving on to the next step. If nobody raises blocking issues, it is time to go ahead and publish the drafted release.
7. |o/ Announce the new release on [Slack](https://envoyproxy.slack.com/archives/CDX3CGTT9).

In case any of the steps above prevent shipping:

1. Discard the draft release tag, if any
2. Close the voting issue, if any. If needed share context in the issue so everyone has context.
3. Subsequently re-spin the full procedure from the top after the blocking issue is resolved

[1] Consider running the unit tests and integration tests repeatedly and concurrently for while.
It's worth noting that some of the integration tests have not been designed for this, so that part
is not tried and proven yet. (some of the integration tests are sensitive to timing, and may get
unstable when slowed down due to concurrent runs). A sample command to do this:

```bash
bazel test \
  --cache_test_results=no \
  --test_env=ENVOY_IP_TEST_VERSIONS=all \
  --runs_per_test=1000 \
  --jobs 50 \
  -c dbg \
  --local_resources 20000,20,0.25 \
  //test:*
```
