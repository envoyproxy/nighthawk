#pragma once

#include <semaphore.h>

#include <functional>
#include <vector>

#include "nighthawk/client/options.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/posix/thread_impl.h"
#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/network/dns_resolver/dns_factory_util.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.h"

#include "source/common/uri_impl.h"

namespace Nighthawk {

/**
 * Creates Envoy bootstrap configuration.
 *
 * The created bootstrap configuration can be used to upstream requests to the
 * specified uris.
 *
 * @param dispatcher is used when resolving hostnames to IP addresses in the
 * bootstrap.
 * @param options are the options this Nighthawk execution was triggered with.
 * @param dns_resolver_factory used to create a DNS resolver to resolve hostnames
 * in the bootstrap.
 * @param typed_dns_resolver_config config used when creating dns_resolver_factory,
 * also needed when creating the resolver.
 * @param number_of_workers indicates how many Nighthawk workers will be
 *        upstreaming requests. A separate cluster is generated for each worker.
 *
 * @return the created bootstrap configuration.
 */
absl::StatusOr<envoy::config::bootstrap::v3::Bootstrap> createBootstrapConfiguration(
    Envoy::Event::Dispatcher& dispatcher, Envoy::Api::Api& api, const Client::Options& options,
    Envoy::Network::DnsResolverFactory& dns_resolver_factory,
    const envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config,
    int number_of_workers);

/**
 * Creates Encapsulation envoy bootstrap configuration.
 *
 * This envoy receives traffic and encapsulates it HTTP
 *
 * @param options are the options this Nighthawk execution was triggered with.
 * @param tunnel_uri URI to the terminating proxy.
 * @param dispatcher is used when resolving hostnames to IP addresses in the
 * bootstrap.
 * @param resolver bootstrap resolver
 *
 * @return the created bootstrap configuration.
 */
absl::StatusOr<envoy::config::bootstrap::v3::Bootstrap>
createEncapBootstrap(const Client::Options& options, UriImpl& tunnel_uri,
                     Envoy::Event::Dispatcher& dispatcher,
                     const Envoy::Network::DnsResolverSharedPtr& resolver);

class EncapsulationSubProcessRunner {
public:
  /**
   * Forks a separate process for Envoy. Both nighthawk and envoy are required to be their own
   * processes
   *
   * @param nighthawk_runner executes nighthawk's workers in current process
   * @param encap_envoy_runner starts up Encapsulation Envoy in a child process.
   * This takes a blocked semaphore which it is responsible for signalling and allowing
   * nighthawk_runner to execute once envoy is ready to serve.
   * Once nighthawk_runner finishes executing, encap_envoy_runner receives a SIGTERM
   *
   */
  EncapsulationSubProcessRunner(std::function<void()> nighthawk_runner,
                                std::function<void(sem_t&)> encap_envoy_runner)
      : nighthawk_runner_(nighthawk_runner), encap_envoy_runner_(encap_envoy_runner) {
    nighthawk_control_sem_ = static_cast<sem_t*>(
        mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0));

    // create blocked semaphore for nighthawk to wait on
    int ret = sem_init(nighthawk_control_sem_, /*pshared=*/1, /*count=*/0);
    if (ret != 0) {
      throw NighthawkException("Could not initialise semaphore");
    }
  };

  ~EncapsulationSubProcessRunner() {
    auto status = TerminateEncapSubProcess();
    if (!status.ok()) {
      ENVOY_LOG_MISC(warn, status.ToString());
    }
    if (pid_ == 0) {
      // Have only parent process destroy semaphore
      sem_destroy(nighthawk_control_sem_);
      munmap(nighthawk_control_sem_, sizeof(sem_t));
    }
  }
  /**
   * Run functions in parent and child processes. It blocks until nighthawk_runner
   * returns.
   *
   * @return error status for processes
   **/
  absl::Status Run() {
    return RunWithSubprocess(nighthawk_runner_, encap_envoy_runner_);
  }

  /**
   * Sends a SIGTERM to Encap Envoy subprocess and blocks till exit
   *
   **/
  absl::Status TerminateEncapSubProcess() {
    Envoy::Thread::LockGuard guard(terminate_mutex_);
    if (pid_ == -1 || pid_ == 0) {
      return absl::OkStatus();
    }

    if (kill(pid_, SIGTERM) == -1 && errno != ESRCH) {
      return absl::InternalError("Failed to kill encapsulation subprocess");
    }

    int status;
    waitpid(pid_, &status, 0);
    pid_ = -1;

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      // Child process did not crash.
      return absl::OkStatus();
    }

    // Child process crashed.
    return absl::InternalError(absl::StrCat("Envoy crashed with code: ", status));
  }

private:
  absl::Status RunWithSubprocess(std::function<void()> nighthawk_runner,
                                 std::function<void(sem_t&)> encap_envoy_runner);

  std::function<void()> nighthawk_runner_;
  std::function<void(sem_t&)> encap_envoy_runner_;
  pid_t pid_ = -1;
  sem_t* nighthawk_control_sem_;
  Envoy::Thread::MutexBasicLockable terminate_mutex_;
};

/**
 * Spins function into thread
 *
 * @param thread_routine executes nighthawk's workers
 *
 * @return thread pointer
 */
Envoy::Thread::PosixThreadPtr createThread(std::function<void()> thread_routine);

} // namespace Nighthawk
