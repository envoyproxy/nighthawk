# Hitting multiple endpoints with a traffic profile

## Description

Below is an example which will send requests to two endpoints (`127.0.0.1:80` and `127.0.0.2:80`), while alternating between two request headers which contain different paths and hosts.

## Practical use

This example has been useful to test a mesh that exposed multiple endpoints, which in turn would offer access to multiple applications via different hosts/paths.

## Features used

This example illustrates the following features:

- [Request Source](https://github.com/envoyproxy/nighthawk/blob/261abb62c40afbdebb317f320fe67f1a1da1838f/api/request_source/request_source_plugin.proto#L15) (specifically the file-based implementation).
- [Multi-targeting](https://github.com/envoyproxy/nighthawk/blob/261abb62c40afbdebb317f320fe67f1a1da1838f/api/client/options.proto#L84)

## Steps

### Configure the file based request source

Place a file called `traffic-profile.yaml` in your current working directory. This will act as your configuration for the file-based request source.


```yaml
options:
  - request_method: 1
    request_headers:
      - { header: { key: ":path", value: "/foo" } }  
      - { header: { key: ":authority", value: "foo.com" } }
  - request_method: 1
    request_headers:
      - { header: { key: ":path", value: "/bar" } }
      - { header: { key: ":authority", value: "bar.com" } }
```

### Configure the CLI

Below is a minimal CLI example which will consume the file based request source configuration created above, and hit multiple endpoints.

```bash
bazel-bin/nighthawk_client --request-source-plugin-config "{name:\"nighthawk.file-based-request-source-plugin\",typed_config:{\"@type\":\"type.googleapis.com/nighthawk.request_source.FileBasedOptionsListRequestSourceConfig\",file_path:\"traffic-profile.yaml\",}}" --multi-target-endpoint 127.0.0.1:80 --multi-target-endpoint 127.0.0.2:80 --multi-target-path /
```
