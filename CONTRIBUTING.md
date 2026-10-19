We welcome contributions from the community. Please read the following guidelines carefully to
maximize the chances of your PR being merged.

# Communication

* Before starting work on a major feature, please reach out to us via [GitHub](https://github.com/envoyproxy/nighthawk) or [Slack](https://envoyproxy.slack.com/archives/CDX3CGTT9). We will make sure no one else is already working on it and ask you to open a GitHub issue.
* Small patches and bug fixes don't need prior communication.

# Coding style

* Coding style mirrors [Envoy's policy](https://github.com/envoyproxy/envoy/blob/main/STYLE.md)

# Breaking change policy

Both API and implementation stability are important to Nighthawk. Since the API is consumed by clients beyond Nighthawk, breaking changes to that are prohibited.

# Submitting a PR

* Generally Nighthawk mirrors [Envoy's policy](https://github.com/envoyproxy/envoy/blob/main/CONTRIBUTING.md#submitting-a-pr) with respect to PR submission policy.
* Any PR that changes user-facing behavior **must** have associated documentation in [docs](docs) as
  well as [release notes](docs/root/version_history.md).

# PR review policy for maintainers

* Generally Nighthawk mirrors [Envoy's policy](https://github.com/envoyproxy/envoy/blob/main/CONTRIBUTING.md#pr-review-policy-for-maintainers) with respect to maintainer review policy.
* See [OWNERS.md](OWNERS.md) for the current list of maintainers.
* It is helpful if you apply the label `waiting-for-review` to any PRs that are ready to be reviewed by a maintainer.
  * Reviewers will change the label to `waiting-for-changes` when responding.

# DCO: Sign your work

Commits need to be signed off. See [here](https://github.com/envoyproxy/envoy/blob/main/CONTRIBUTING.md#dco-sign-your-work).


## Triggering CI re-run without making changes

PR authors can trigger a re-run of CI jobs by commenting on their Pull Request:

* Comment `/retest` to re-run only failed jobs for the latest commit.
* Comment `/retest all` to re-run all jobs from scratch (including passed ones).
* Comment `/retest <job-name>` (e.g. `/retest tsan`, `/retest test_gcc`) to re-run a specific completed job.

See also [Envoy's policy](https://github.com/envoyproxy/envoy/blob/main/CONTRIBUTING.md#triggering-ci-re-run-without-making-changes).
