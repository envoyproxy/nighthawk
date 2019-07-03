#pragma once

#include <grpc++/grpc++.h>

#include <chrono>

#include "nighthawk/common/exception.h"

#include "common/utility.h"

#include "client/service_impl.h"

#include "api/client/service.pb.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

class ServiceMain {
public:
  ServiceMain(int argc, const char** argv);
  void Run();
  void Shutdown();

private:
  Envoy::Network::Address::InstanceConstSharedPtr listener_address_;
  ServiceImpl service_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub_;
};

} // namespace Client
} // namespace Nighthawk
