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
# build it. for best accuracy it is important to specify -c opt.
bazel build -c opt //:nighthawk_client

## Using the Nighthawk client

```
➜ bazel-bin/nighthawk_client --help

USAGE: 

   bazel-bin/nighthawk_client  [--output-format <human|yaml
                                        |json>] [-v <trace|debug|info|warn
                                        |error|critical>] [--concurrency
                                        <string>] [--h2] [--timeout
                                        <uint64_t>] [--duration <uint64_t>]
                                        [--connections <uint64_t>] [--rps
                                        <uint64_t>] [--] [--version] [-h]
                                        <uri format>


Where: 

   --output-format <human|yaml|json>
     Verbosity of the output. Possible values: [human, yaml, json]. The
     default output format is 'human'.

   -v <trace|debug|info|warn|error|critical>,  --verbosity <trace|debug
      |info|warn|error|critical>
     Verbosity of the output. Possible values: [trace, debug, info, warn,
     error, critical]. The default output format is 'info'.

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
     maximally use. Default: 1.

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

benchmark_http_client.queue_to_connect: 9997 samples, mean: 0.000010513s, pstdev: 0.000007883s
Percentile  Count       Latency        
0           1           0.000006549s   
0.5         4999        0.000007325s   
0.75        7498        0.000010387s   
0.8         7998        0.000012010s   
0.9         8998        0.000017883s   
0.95        9498        0.000025680s   
0.990625    9905        0.000047415s   
0.999023    9988        0.000076479s   
1           9997        0.000113999s   

benchmark_http_client.request_to_response: 9997 samples, mean: 0.000156183s, pstdev: 0.000147691s
Percentile  Count       Latency        
0           1           0.000055659s   
0.5         5003        0.000146655s   
0.75        7500        0.000152631s   
0.8         7998        0.000161455s   
0.9         8998        0.000205247s   
0.95        9498        0.000269951s   
0.990625    9904        0.000495695s   
0.999023    9988        0.000971967s   
1           9997        0.008059903s   

sequencer.blocking: 42 samples, mean: 0.000755961s, pstdev: 0.001613017s
Percentile  Count       Latency        
0           1           0.000067199s   
0.5         21          0.000117843s   
0.75        32          0.000540191s   
0.8         34          0.000544223s   
0.9         38          0.000653183s   
0.95        40          0.003944319s   
1           42          0.006995967s   
1           42          0.006995967s   
1           42          0.006995967s   

sequencer.callback: 9997 samples, mean: 0.000173003s, pstdev: 0.000153462s
Percentile  Count       Latency        
0           1           0.000066339s   
0.5         4999        0.000157879s   
0.75        7498        0.000164863s   
0.8         7998        0.000179951s   
0.9         8998        0.000234383s   
0.95        9498        0.000314831s   
0.990625    9904        0.000558047s   
0.999023    9988        0.001160063s   
1           9997        0.008074751s   

Counter                                 Value       Per second
client.benchmark.http_2xx               9999        1999.80
client.upstream_cx_http1_total          2           0.40
client.upstream_cx_rx_bytes_total       36026397    7205279.40
client.upstream_cx_total                2           0.40
client.upstream_cx_tx_bytes_total       599940      119988.00
client.upstream_rq_pending_total        2           0.40
client.upstream_rq_total                9999        1999.80
```

Nighthawk will create a directory called `measurements/` and log results in json format there.
The name of the file will be `<epoch.json>`, which contains:

- The start time of the test, and a serialization of the Nighthawk options involved.
- The mean latency and the observed standard deviation.
- Latency percentiles produced by HdrHistogram.
- Counters as tracked by Envoy's connection-pool and ssl libraries.

## Accuracy and repeatability considerations when using the Nighthawk client.

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

```
sudo tuned-adm profile network-latency
```
- When using Nighthawk with concurrency > 1 or multiple connections, workers may produce significantly different results. That can happen because of various reasons:
  - Server fairness. For example, connections may end up being serviced by the same server thread, or not.
  - One of the clients may be unlucky and structurally spend time waiting on requests from the other(s)
    being serviced due to interference of request release timings and server processing time.
- Consider using separate machines for the clients and server(s).
