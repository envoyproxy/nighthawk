# Nighthawk

*A L7 HTTP protocol family benchmarking tool based on Envoy*

## Current state

The nighthawk client supports HTTP/1.1 and HTTP/2 over HTTP and HTTPS.

HTTPS certificates are not yet validated.

## Prerequisites

### Ubuntu

First, follow steps 1 and 2 over at [Quick start Bazel build for developers](https://github.com/envoyproxy/envoy/blob/master/bazel/README.md#quick-start-bazel-build-for-developers).

## Building and testing Nighthawk

```bash
# test it
bazel test -c fastbuild //test:nighthawk_test
```

### Build it

```bash
# for best accuracy it is important to specify -c opt.
bazel build -c opt //:nighthawk_client
```

### Using the Nighthawk client

```bash
➜ bazel-bin/nighthawk_client --help

USAGE: 

   bazel-bin/nighthawk_client  [--request-body-size <uint32_t>]
                               [--request-header <string>] ... 
                               [--request-method <GET|HEAD|POST|PUT|DELETE
                               |CONNECT|OPTIONS|TRACE>] [--address-family
                               <auto|v4|v6>] [--burst-size <uint64_t>]
                               [--prefetch-connections] [--output-format
                               <human|yaml|json>] [-v <trace|debug|info
                               |warn|error|critical>] [--concurrency
                               <string>] [--h2] [--timeout <uint64_t>]
                               [--duration <uint64_t>] [--connections
                               <uint64_t>] [--rps <uint64_t>] [--]
                               [--version] [-h] <uri format>


Where: 

   --request-body-size <uint32_t>
     Size of the request body to send. NH will send a number of consecutive
     'a' characters equal to the number specified here. (default: 0, no
     data).

   --request-header <string>  (accepted multiple times)
     Raw request headers in the format of 'name: value' pairs. This
     argument may specified multiple times.

   --request-method <GET|HEAD|POST|PUT|DELETE|CONNECT|OPTIONS|TRACE>
     Request method used when sending requests. The default is 'GET'.

   --address-family <auto|v4|v6>
     Network addres family preference. Possible values: [auto, v4, v6]. The
     default output format is 'v4'.

   --burst-size <uint64_t>
     Release requests in bursts of the specified size (default: 0, no
     bursting).

   --prefetch-connections
     Prefetch connections before benchmarking (HTTP/1 only).

   --output-format <human|yaml|json>
     Verbosity of the output. Possible values: [human, yaml, json]. The
     default output format is 'human'.

   -v <trace|debug|info|warn|error|critical>,  --verbosity <trace|debug
      |info|warn|error|critical>
     Verbosity of the output. Possible values: [trace, debug, info, warn,
     error, critical]. The default level is 'info'.

   --concurrency <string>
     The number of concurrent event loops that should be used. Specify
     'auto' to let Nighthawk leverage all vCPUs that have affinity to the
     Nighthawk process.Note that increasing this results in an effective
     load multiplier combined with the configured-- rps and --connections
     values.Default : 1. 

   --h2
     Use HTTP/2

   --timeout <uint64_t>
     Timeout period in seconds used for both connection timeout and grace
     period waiting for lagging responses to come in after the test run is
     done. Default: 5.

   --duration <uint64_t>
     The number of seconds that the test should run. Default: 5.

   --connections <uint64_t>
     The number of connections per event loop that the test should
     maximally use. HTTP/1 only. Default: 1.

   --rps <uint64_t>
     The target requests-per-second rate. Default: 5.

   --,  --ignore_rest
     Ignores the rest of the labeled arguments following this flag.

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.

   <uri format>
     (required)  uri to benchmark. http:// and https:// are supported, but
     in case of https no certificates are validated.


   Nighthawk, a L7 HTTP protocol family benchmarking tool based on Envoy.


```

## Sample benchmark run

```bash
# start the benchmark target (Envoy in this case) on core 3.
$ taskset -c 3 /path/to/envoy --config-path nighthawk/tools/envoy.yaml

# run a quick benchmark using cpu cores 4 and 5.
$ taskset -c 4-5 bazel-bin/nighthawk_client --rps 1000 --concurrency auto http://127.0.0.1:10000/

Nighthawk - A layer 7 protocol benchmarking tool.

benchmark_http_client.queue_to_connect: 9993 samples, mean: 0.000010053s, pstdev: 0.000011278s
Percentile  Count       Latency        
0           1           0.000006713s   
0.5         4997        0.000007821s   
0.75        7495        0.000008677s   
0.8         7995        0.000009084s   
0.9         8994        0.000011583s   
0.95        9494        0.000015702s   
0.990625    9900        0.000077299s   
0.999023    9984        0.000145863s   
1           9993        0.000232383s   

benchmark_http_client.request_to_response: 9993 samples, mean: 0.000115456s, pstdev: 0.000052326s
Percentile  Count       Latency        
0           1           0.000080279s   
0.5         4998        0.000104799s   
0.75        7496        0.000113787s   
0.8         7996        0.000121359s   
0.9         8994        0.000153487s   
0.95        9494        0.000180647s   
0.990625    9900        0.000382591s   
0.999023    9984        0.000608159s   
1           9993        0.000985951s   

sequencer.blocking: 14 samples, mean: 0.000531127s, pstdev: 0.000070919s
Percentile  Count       Latency        
0           1           0.000484127s   
0.5         7           0.000495615s   
0.75        11          0.000521007s   
0.8         12          0.000545887s   
0.9         13          0.000655839s   
1           14          0.000736223s   

sequencer.callback: 9993 samples, mean: 0.000131079s, pstdev: 0.000060199s
Percentile  Count       Latency        
0           1           0.000091547s   
0.5         4998        0.000116935s   
0.75        7495        0.000127351s   
0.8         7995        0.000137807s   
0.9         8994        0.000174335s   
0.95        9495        0.000210063s   
0.990625    9900        0.000444063s   
0.999023    9984        0.000664383s   
1           9993        0.001103615s   

Counter                                 Value       Per second
client.benchmark.http_2xx               9995        1999.00
client.upstream_cx_close_notify         98          19.60
client.upstream_cx_http1_total          100         20.00
client.upstream_cx_rx_bytes_total       8585215     1717043.00
client.upstream_cx_total                100         20.00
client.upstream_cx_tx_bytes_total       569715      113943.00
client.upstream_rq_pending_total        100         20.00
client.upstream_rq_total                9995        1999.00
```

## Accuracy and repeatability considerations when using the Nighthawk client

- Processes not related to the benchmarking task at hand may add significant noise. Consider stopping any
  processes that are not needed. 
- Be aware that power state management and CPU Frequency changes are able to introduce significant noise.
  When idle, Nighthawk uses a busy loop to achieve precise timings when starting requests, which helps minimize this.
  Still, consider disabling C-state changes in the system BIOS.
- Be aware that CPU thermal throttling may skew results.
- Consider using `taskset` to isolate client and server. On machines with multiple physical CPUs there is a choice here.
  You can partition client and server on the same physical processor, or run each of them on a different physical CPU. Be aware of the latency effects of interconnects such as QPI.
- Consider disabling hyper-threading.
- Consider tuning the benchmarking system for low (network) latency. You can do that manually, or install [tuned](http://manpages.ubuntu.com/manpages/bionic/man8/tuned-adm.8.html) and run:

| As this may change boot flags, take precautions, and familiarize yourself with the tool on systems that you don't mind breaking. For example, running this has been observed to mess up dual-boot systems! |
| --- |

```bash
sudo tuned-adm profile network-latency
```

- When using Nighthawk with concurrency > 1 or multiple connections, workers may produce significantly different results. That can happen because of various reasons:
  - Server fairness. For example, connections may end up being serviced by the same server thread, or not.
  - One of the clients may be unlucky and structurally spend time waiting on requests from the other(s)
    being serviced due to interference of request release timings and server processing time.
- Consider using separate machines for the clients and server(s).
