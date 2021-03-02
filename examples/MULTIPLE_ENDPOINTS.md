# Hitting multiple endpoints with a traffic profile

## Description

Below is an example which will send requests to two endpoints `127.0.0.1:80` and `127.0.0.2:80`, while alternating over two request headers, which contain different paths and hosts.

## Define a traffic profile

Place a file called `traffic-profile.yaml` in your current working directory.

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

## Configuring the CLI

Below is a CLI examples which will consume the traffic profile created above, and send it to multiple endpoints.

```bash
bazel-bin/nighthawk_client --request-source-plugin-config "{name:\"nighthawk.file-based-request-source-plugin\",typed_config:{\"@type\":\"type.googleapis.com/nighthawk.request_source.FileBasedOptionsListRequestSourceConfig\",file_path:\"traffic-profile.yaml\",}}" -v trace --multi-target-endpoint 127.0.0.1:80 --multi-target-endpoint 127.0.0.2:80 --multi-target-path / --duration 1
```
