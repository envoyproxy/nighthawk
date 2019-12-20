# Nighthawk: architecture and key concepts

## High level interaction model

**Process** creates one or more **Workers**. **Worker** will run **Sequencer**, which in turn queries **RateLimiter** for request-release timings. When it is time to release a request, **BenchmarkClient** will be requested to do so by **Sequencer**. **BenchMarkClient** will then ask its underlying **Pool** to create a **StreamDecoder** release the actual request. **StreamDecoder** will query the request data it needs to send from the configured **RequestSource**, and send it off. **StreamDecoder** will emit events as it progresses (pool ready, completion, etc), and timings will subsequently be recorded into **Statistic** as well as get bubbled up to **Sequencer** for tracking in-flight work and **Statistic** bookkeeping.

**Sequencer** will query the configured **TerminationPredicates** to terminate when and how to terminate execution. When all **Workers** have finished, **Process** will collect the results from all **Workers** via **OutputCollector**, and run **OutputFormatter** to transform to the requested output format.

## Notable upcoming changes

Calling out two new concepts that may get proposed in the future, and cause some churn in the code base as we inject them.

### Phases

One notable addition / change that may get proposed in the near future is the introduction of **Phase**. **Phase** would represent a distinct stage of an execution, like, for example, *warmup***.** It would then be useful to have per-phase reporting of latencies as well as counters and latencies. 

Concretely, a warm-up phase could be represented by a distinct duration termination predicate and a ramping rate limiter. Upon completion, the `hot` BenchmarkClient with its associated pool would then be live-transferred to the next configured phase, after which execution can continue.

One other reason to have this is that it would enable remote- and/or cli- controlled ramping of certain test parameters by associating those to different phases. Termination predicates can be leveraged to immediately terminate the current phase after injecting a new one, allowing for real-time steering via gRPC and/or CLI.

### Streaming parameterizations and output stats

One we have phases, the gRPC service, and perhaps the CLI, would be natural candidates to follow up with to allow dynamic phase injection, as well as send back reports per phase.

## Key concept descriptions

### Process

**Process** represents the primary entry point to a Nighthawk execution run. Only one Process is allowed to exist at the same point in time within an OS Process. **Process **is responsible for performing process-wide initialization and termination, as well as handle input configuration, deliver output, and co-ordinate Workers. **ProcessImpl **is re-used accross the CLI and the gRPC service.

### Worker

**Worker **is responsible for performing correct initialization and termination of its thread, as well as execution of it’s designated task and offering a way for consumers to wait for that task to complete. 

### TerminationPredicate

**TerminationPredicate **is responsible for if and how to terminate. As of today, there are two types: one that will indicate that it is time to terminate based on a pre-configured duration, and one that will do so based on absolute counter thresholds. 

### Sequencer

**SequencerImpl** resides on a worker-thread, and drives itself via timers that run on the dispatcher, and coordinates interaction between **RateLimiter,** **BenchmarkClient**, and **TerminationPredicate** to drive execution to completion.

### RateLimiter

**RateLimiter **is responsible for indicating when it is time to release a request. **RateLimiter **offers a semaphore-like interaction model, as in closed-loop mode it may be that **BenchmarkClient **is not able to satisfy request-release timings, in which case acquisitions from **RateLimiter** need to be cancelled. Concretely, as of today there is **LinearRateLimiterImpl** which offers a straight-paced plain frequency, as well as work in progress on **DistributionSamplingRateLimiterImpl **(adding uniformly distributed random timing offsets to an underlying **RateLimiter**) and **RampingRateLimiter**. 

### BenchmarkClient

As of today, there’s a single implementation called** BenchmarkClientImpl,** which wraps Envoy’s **Upstream** concept and (slightly) customized H1/H2 **Pool** concepts. For executing requests, the pool will be requested to create a **StreamEncoder**, and Nighthawk will pass its own **StreamDecoderImpl** into that as an argument. The integration surface between **BenchmarkClient **is defined via **BenchmarkClient::tryStartRequest()** and a callback specification which will be fired upon completion of a successfully started request.

For H3, it is anticipated that it will fit into this model, but if all else fails, it will be entirely possible to wire in a new type of **BenchmarkClient.**

### RequestSource

**RequestSource **is an abstraction that allows us to implement different ways for **BenchmarkClient** to get information on what the request that it is about to fire off should look like. Today, two implementations exist: a static one, which will repeat the same request over and over, as well as one that pulls dynamic request data from a grpc service. The latter can, for example, be used to implement log-replay.

### StreamDecoder

StreamDecoder is a Nighthawk-specific implementation of an Envoy concept. It is responsible for coordinating lifetime events of a request to upper abstraction layers (**BenchmarkClient**, **Sequencer) **as well as latency recording.

### OutputCollector

OutputCollector is a container that facilitates building up the native output format of Nighthawk (proto3, nighthawk::client::Output). It is the basis for all output formats offered by Nighthawk, including CLI human output.

### OutputFormatter

OutputFormatter is responsible for transformations of nighthawk::client::Output to requested formats (e.g. human, json, fortio, etc)

### Statistic

Nighthawk’s statistic is responsible for administrating latencies. The most notable implemenation that exists today wraps HdrHistogram, but Nighthawk also has a couple of other implementations which mostly exist to ensure that floating point math is correct in tests. For various reasons, HdrHistogram might get replaced by [libcirclhist](https://github.com/envoyproxy/nighthawk/issues/115) in the near future.

### H1Pool & H2Pool

Nighthawk derives its own version of these from the vanilla Envoy ones. It does that to implement things like pro-active connection prefetching and H2 multi-connection support, as well as offer more connection management strategies. 

## Executables

### nighthawk_client

This is the CLI interface

### nighthawk_service

Nighthawk’s gRPC service is able to execute load tests, and also offers a stub request source gRPC service method implementation. 

The stub request source gRPC method can be used for testing remote request source configuration (either with nighthawk_client, or by configuring request-source to loop back when requesting nighthawk_service to perform a load test). 

While the stub gRPC method has no production use today, perhaps in the future this service method may get configurable so it can read traffic streams from disk, or from yet another remote source (which would allow the gRPC service to serve as a workload distributor for its Workers in a benchmark execution, as well as be convenient for those implementing a gRPC request source as they only have a single inbound gRPC call instead of one per worker).

### nighthawk_test_server

Nighthawk’s test server, based on Envoy. It is able to synthesize delays and responses based on configuration via request headers (next to on-disk configuration).

### nighthawk_output_transform

Utility for transfoming the nighthawk-native "lossless" json output format into other formats (e.g. whuman, fortio). It can be very useful to always store the json output format, yet be able to easily get to one of the other output formats. It’s like having the cake and eating it too!