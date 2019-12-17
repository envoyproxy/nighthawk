We welcome contributions from the community. Please read the following guidelines carefully to
maximize the chances of your PR being merged.

# Communication

* Before starting work on a major feature, please reach out to us via GitHub, Slack,
  email, etc. We will make sure no one else is already working on it and ask you to open a
  GitHub issue.
* Small patches and bug fixes don't need prior communication.

# Coding style

* Coding style mirrors [Envoy's policy](https://github.com/envoyproxy/envoy/blob/master/STYLE.md)

# Breaking change policy

Both API and implementation stability are important to Nighthawk. Since the API is consumed by clients beyond Nighthawk, breaking changes to that are prohibited.

# Submitting a PR

* Generally Nighthawk mirrors [Envoy's policy](https://github.com/envoyproxy/envoy/blob/master/CONTRIBUTING.md#pr-review-policy-for-maintainers) with respect to PR submission policy.
* Any PR that changes user-facing behavior **must** have associated documentation in [docs](docs) as
  well as [release notes](docs/root/intro/version_history.rst). 

# PR review policy for maintainers

* Generally Nighthawk mirros [Envoy's policy](https://github.com/envoyproxy/envoy/blob/master/CONTRIBUTING.md#pr-review-policy-for-maintainers) with respect to maintainer review policy.
* See [OWNERS.md](OWNERS.md) for the current list of maintainers.

# DCO: Sign your work

Commits need to be signed off. [See here](https://github.com/envoyproxy/envoy/blob/master/CONTRIBUTING.md#dco-sign-your-work).


## Triggering CI re-run without making changes

See [here](https://github.com/envoyproxy/envoy/blob/master/CONTRIBUTING.md#triggering-ci-re-run-without-making-changes).