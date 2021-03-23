# Header based latency tracking

##  Tracking latencies contained in response headers.

The api allows one to [point out a response header](https://github.com/envoyproxy/nighthawk/blob/211b3b53f60d5ed3855d15eb8a4c2d7a3edc0724/api/client/options.proto#L222) whose values will be interpreted as latencies, to be tracked in a histogram by the client.

A sample invokation of the CLI to track latencies communicated via a response header called `x-origin-request-receipt-delta` looks like:

```bash
./nighthawk_client --latency-response-header-name x-origin-request-receipt-delta http://foo/
```

## Tracking upstream processing time

One use case for tracking latencies via response headers, is measuring upstream processing time. For example, Envoy proxy can be set up to emit `x-envoy-upstream-service-time`, which allows tracking that as a component of the overal request/reply time. Also, when using synhetic delays on the test server, the theoretical expected distribution of the synthetic delays should ideally closely match the actual observed response latencies, and any unexpected divergence might be an interesting data point.

## Tracking request-arrival timing deltas using the test server

Another use-case is tracking request-arrival time deltas using a feature of the test server.

```yaml
# The time-tracking extension will emit request-arrival timing deltas in a response header.
    http_filters:
    - name: time-tracking
    typed_config:
        "@type": type.googleapis.com/nighthawk.server.ResponseOptions
        emit_previous_request_delta_in_response_header: x-origin-request-receipt-delta
    - name: test-server
    typed_config:
        "@type": type.googleapis.com/nighthawk.server.ResponseOptions
        response_body_size: 10
```

The resulting histogram of arrival timing-deltas can be compared to the configured distribution of request-release timings. In an ideal world, 1000qps would yield a flat 1ms delta, and adding uniform jitter would yield perfect bell curve / normal distribution.

A real world example of the output for this using straight 1000 qps linear pacing in a baseline test against the test server looks like:

```
  0.5         2519        0s 000ms 999us 
  0.75        3767        0s 001ms 001us 
  0.8         4002        0s 001ms 002us 
  0.9         4508        0s 001ms 003us 
  0.95        4756        0s 001ms 004us 
  0.990625    4953        0s 001ms 005us 
  0.99902344  4995        0s 001ms 015us
```

The way this changes when introducing a proxy can be interesting when comparing to baseline, as this divergence may say something about distortion introduced on the path from the client to the test server, and perhaps can even be used to quantify that.

