# Changelog

All breaking changes to this project will be documented in this file, most recent changes at the top.

## 0.5.0

In an effort to clean up the previous change and improve it, we re-modified the dynamic-delay and time-tracking filters to extricate their configuration entirely from nighthawk.server.ResponseOptions.

If you are converting from the previous configuration with `experimental_response_options`, such as:

```
          http_filters:
          - name: time-tracking
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.TimeTrackingConfiguration
              experimental_response_options:
                emit_previous_request_delta_in_response_header: x-origin-request-receipt-delta
          - name: dynamic-delay
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.DynamicDelayConfiguration
              experimental_response_options:
                static_delay: 1.33s
          - name: test-server
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.ResponseOptions
              response_body_size: 10
              static_delay: 1.33s
              emit_previous_request_delta_in_response_header: x-origin-request-receipt-delta
              v3_response_headers:
              - { header: { key: "x-nh", value: "1"}}
```

You should now specify only fields related to each filter in their configuration, and you can do so at the top-level of those protos:

```
          http_filters:
          - name: time-tracking
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.TimeTrackingConfiguration
              emit_previous_request_delta_in_response_header: x-origin-request-receipt-delta
          - name: dynamic-delay
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.DynamicDelayConfiguration
              static_delay: 1.33s
          - name: test-server
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.ResponseOptions
              response_body_size: 10
              v3_response_headers:
              - { header: { key: "x-nh", value: "1"}}
```

This change does NOT affect how headers update the base configurations.

## 0.4.0

Due to [upstream envoy change](https://github.com/envoyproxy/nighthawk/commit/4919c54202329adc3875eb1bce074af33d54e26d), we modified the dynamic-delay and time-tracking filters' configuration protos to have their own configuration protos wrapping nighthawk.server.ResponseOptions.

For base yaml configuration files, if you previously had:

```
          http_filters:
          - name: time-tracking
          - name: dynamic-delay
          - name: test-server
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.ResponseOptions
              response_body_size: 10
              static_delay: 1.33s
              emit_previous_request_delta_in_response_header: x-origin-request-receipt-delta
              v3_response_headers:
              - { header: { key: "x-nh", value: "1"}}
```

You should now explicitly specify the types and values of the protos as such:

```
          http_filters:
          - name: time-tracking
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.TimeTrackingConfiguration
              experimental_response_options:
                emit_previous_request_delta_in_response_header: x-origin-request-receipt-delta
          - name: dynamic-delay
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.DynamicDelayConfiguration
              experimental_response_options:
                static_delay: 1.33s
          - name: test-server
            typed_config:
              "@type": type.googleapis.com/nighthawk.server.ResponseOptions
              response_body_size: 10
              static_delay: 1.33s
              emit_previous_request_delta_in_response_header: x-origin-request-receipt-delta
              v3_response_headers:
              - { header: { key: "x-nh", value: "1"}}
```

This change does NOT affect how headers update the base configurations.