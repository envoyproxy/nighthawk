---
name: Non-{crash,security} bug
about: Bugs which are not crashes, DoS or other security issue
title: ''
labels: bug,triage
assignees: ''

---

*Title*: *One line description*

*Description*:
>What issue is being seen? Describe what should be happening instead of
the bug, for example: Nighthawk should not crash, the expected value isn't
returned, etc.

*Reproduction steps*:
> Include sample requests, environment, etc. All data and inputs
required to reproduce the bug.

>**Note**: If there are privacy concerns, sanitize the data prior to
sharing.

*Logs*:
>Include the Nighthawk logs.

>**Note**: If there are privacy concerns, sanitize the data prior to
sharing.

*Call Stack*:
> If the Envoy binary is crashing, a call stack is **required**.
Please refer to the [Bazel Stack trace documentation](https://github.com/envoyproxy/envoy/tree/master/bazel#stack-trace-symbol-resolution).