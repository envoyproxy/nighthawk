# WIP - Benchmarking testsuite

The NH benchmark test suite builds on top Nighthawk's integration test framework, and
can be used to scaffold tests and obtain latency reports as well as flamegraphs.

## GOAL

The goal is te be able to:
- run the suite against arbitrary Envoy revisions
- persist profile dumps, flamegraphs, and latency numbers per test in gcp


## Building the suite

```
bazel build -c opt //benchmarks:*
```

## Testing the suite

```bash
bazel test --test_env=ENVOY_IP_TEST_VERSIONS=v4only //benchmarks:*
```

## Using the suite

To use the benchmark suite, there are a couple of environment variables that need
to be overriden:

- `ENVOY_IP_TEST_VERSIONS` (optional), e.g. v4only|v6only
- `NH_CERTDIR` points the suite to the Envoy test ssl certificate directory.
- `NH_RUNDIR` points to the directory where the nighthawk binaries exist
- `NH_CONFDIR` points the suite to the backend configuration files that belong
  to any pre-provided test fixtures used to bootstrap test servers.

# TODO
- Facilitate injection of an Envoy running in a docker container between the test client and test server. 
  We could add a helper to the test fixture that accepts an Envoy sha, and reroutes traffic behind the
  scenes through the Envoy docker build that is associated to the passed in revision.
- Copy out the artifacts and push those to a gcp bucket. Current status:
   - cpu profiles are dumped to tmp per test (named according to the test). ideally we'd
     also dump flamegraph svg's
   - raw json is send to the output on stderr. ideally we'd persist in fortio format,
     raw yaml/json, and human readable output.
- Allow pointing out a directory for the suite to scaffold tests, thereby overriding the 
  stock suite that we come with. This allows consumers to script their own testsuite.
- A UI -- though we may be able to get by with just a uri structure conventioned around the envoy
  sha. e.g. http://perf-ci-host/gcpsync/[envoy-sha]-[timestamp]/ to link CI, and directory index the 
  artifacts.

# FUTURE

- Allow scavenging a separate repo for tests
- profiling / flamegraphing via perf/bcc tools; include the proxy-wasm flamegraphing research r&d
- Allow injection of other proxies: nginx, haproxy
- an app that integrates fortios UI, pprof's web UI

# Sample 

Lets assume we have a test_xxx.py containing something like

```python
def run_with_cpu_profiler(fixture,
                          rps=999999,
                          use_h2=False,
                          etc...):
  assert (fixture.test_server.enableCpuProfiler())
  MIN_EXPECTED_REQUESTS = 100
  args = [
      fixture.getTestServerRootUri(), "--rps",
      str(rps), "--duration",
  parsed_json, _ = fixture.runNighthawkClient(args)
  counters = fixture.getNighthawkCounterMapFromJson(parsed_json)
  # sample expectations (optional)
  assertCounterGreater(counters, "benchmark.http_2xx", MIN_EXPECTED_REQUESTS)
  .. etc ...
  # dump output
  logging.info(fixture.transformNighthawkJsonToHumanReadable(json.dumps(parsed_json)))

# Actual test
def test_http_h2_1mb_request_small_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)

```

Run it:

```
export ENVOY_IP_TEST_VERSIONS=v4only
export NH_RUNDIR="$(pwd)/bazel-bin/"
export NH_CERTDIR="${NH_RUNDIR}benchmarks/benchmarks.runfiles/nighthawk/external/envoy/test/config/integration/certs/"
export NH_CONFDIR="${NH_RUNDIR}/benchmarks/benchmarks.runfiles/nighthawk/test/integration/configurations/"

bazel-bin/benchmarks/benchmarks
```

```bash
benchmarks/test_benchmark.py::test_http_h2_1mb_request_small_reply[IpVersion.IPV4]                    
------------------------------------------------------------------------------------------------------------------- live log setup ------------------------------------------------------------------------------------------------------------------$                                                                                                                                                                    
INFO     root:nighthawk_test_server.py:45 Parameterized server configuration: admin:                                                                                                                         
                                            access_log_path: /tmp/admin_access-test_http_h2_1mb_request_small_reply_IpVersion.IPV4.log                                                                                                                                                                                                                                                                                    
                                            profile_path: /tmp/envoy-test_http_h2_1mb_request_small_reply_IpVersion.IPV4.prof                                                                                                                                                                                                                                                                                             
                                            address:                                                                                                                                                                                                                                                                                                                                                                      
                                              socket_address: { address: 127.0.0.1, port_value: 0 }                                                                                                          
                                          static_resources:                                           
                                            listeners:                                                                                                                                                                                                                                                                                                                                                                    
                              WARNING  root:integration_test_fixtures.py:215 Nighthawk client stderr: [[2020-05-19 12:49:29.273][30280][warning][misc] Using deprecated option 'nighthawk.client.CommandLineOptions.tls_context' from file options.proto. This configuration will be removed from Envoy soon. Please see https://www.envoyproxy.io/docs/envoy/latest/version_history/version_history for details.
                                               [12:49:29.277907][30280][I] Starting 1 threads / event loops. Test duration: 10 seconds.                                                                      
                                               [12:49:29.277929][30280][I] Global targets: 1 connections and 999999 calls per second.                                                                        
                                               [12:49:39.780122][30285][I] Stopping after 10002 ms. Initiated: 4216 / Completed: 4215. (Completion rate was 421.41 per second.)                              
                                               [12:49:40.054199][30280][I] Done.                      
                                               ]                                                      
INFO     root:integration_test_fixtures.py:226 Nighthawk output transform popen() args: [['bazel-bin/nighthawk_output_transform', '--output-format', 'human']]                                               
INFO     root:integration_test_fixtures.py:228 Nighthawk client popen() args: [['bazel-bin/nighthawk_output_transform', '--output-format', 'human']]                                                         
INFO     root:test_benchmark.py:69 Nighthawk - A layer 7 protocol benchmarking tool.                  
                                                                                                      
                                   Queueing and connection setup latency (4216 samples)               
                                     min: 0s 000ms 006us | mean: 0s 000ms 009us | max: 0s 009ms 628us | pstdev: 0s 000ms 148us                                                                               
                                                                                                      
                                     Percentile  Count       Value                                    
                                     0.5         2121        0s 000ms 007us                           
                                     0.75        3167        0s 000ms 007us                           
                                     0.8         3382        0s 000ms 007us                           
                                     0.9         3799        0s 000ms 007us                           
                                     0.95        4008        0s 000ms 007us                           
                                     0.990625    4178        0s 000ms 008us                           
                                     0.999023    4212        0s 000ms 018us                           
                                                                                                      
                                   Request start to response end (4215 samples)                                                                                                                              
                                     min: 0s 002ms 110us | mean: 0s 002ms 354us | max: 0s 005ms 863us | pstdev: 0s 000ms 135us                                                                               
                                                                                                      
                                     Percentile  Count       Value                                    
                                     0.5         2108        0s 002ms 403us                                                                                                                                                                                                                                                                                                                                               
                                     0.75        3162        0s 002ms 434us                                                                                                                                                                                                                                                                                                                                               
                                     0.8         3375        0s 002ms 439us                           
                                     0.9         3795        0s 002ms 451us                                                                                                                                                                                                                                                                                                                                               
                                     0.95        4006        0s 002ms 462us                           
                                     0.990625    4176        0s 002ms 504us                           
                                     0.999023    4211        0s 003ms 385us                           
                                                                                                      
                                   Response body size in bytes (4215 samples)                         
                                     min: 10 | mean: 10.0 | max: 10 | pstdev: 0.0                     
                                                                                                      
                                   Response header size in bytes (4215 samples)                       
                                     min: 97 | mean: 97.0 | max: 97 | pstdev: 0.0                     
                                                                                                      
                                   Blocking. Results are skewed when significant numbers are reported here. (4216 samples)                                                                                   
                                     min: 0s 002ms 125us | mean: 0s 002ms 372us | max: 0s 015ms 538us | pstdev: 0s 000ms 238us                                                                               
                                                                                                      
                                     Percentile  Count       Value                                    
                                     0.5         2109        0s 002ms 418us                           
                                     0.75        3167        0s 002ms 449us                           
                                     0.8         3376        0s 002ms 454us                           
                                     0.9         3799        0s 002ms 466us                           
                                     0.95        4007        0s 002ms 478us                           
                                     0.990625    4177        0s 002ms 522us                           
                                     0.999023    4212        0s 003ms 425us                           
                                                                                                      
                                   Initiation to completion (4215 samples)                            
                                     min: 0s 002ms 121us | mean: 0s 002ms 367us | max: 0s 015ms 524us | pstdev: 0s 000ms 238us                                                                               
                                                                                                      
                                     Percentile  Count       Value                                    
                                     0.5         2109        0s 002ms 414us                           
                                     0.75        3163        0s 002ms 444us                           
                                     0.8         3372        0s 002ms 450us                           
                                     0.9         3797        0s 002ms 462us                           
                                     0.95        4005        0s 002ms 473us                           
                                     0.990625    4176        0s 002ms 516us                           
                                     0.999023    4211        0s 003ms 409us                           
                                                                                                      
                                   Counter                                 Value       Per second                                                                                                            
                                   benchmark.http_2xx                      4215        421.41                                                                                                                
                                   cluster_manager.cluster_added           1           0.10           
                                   default.total_match_count               1           0.10           
                                   membership_change                       1           0.10           
                                   runtime.load_success                    1           0.10           
                                   runtime.override_dir_not_exists         1           0.10           
                                   ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256 1           0.10           
                                   ssl.curves.X25519                       1           0.10           
                                   ssl.handshake                           1           0.10           
                                   ssl.sigalgs.rsa_pss_rsae_sha256         1           0.10           
                                   ssl.versions.TLSv1.2                    1           0.10           
                                   upstream_cx_http1_total                 1           0.10           
                                   upstream_cx_rx_bytes_total              573240      57311.82                                                                                                              
                                   upstream_cx_total                       1           0.10           
                                   upstream_cx_tx_bytes_total              4216518568  421562231.32                                                                                                          
                                   upstream_rq_pending_total               1           0.10           
                                   upstream_rq_total                       4216        421.51                                                                                                                
                                                                                                      
PASSED                                                                                  
```