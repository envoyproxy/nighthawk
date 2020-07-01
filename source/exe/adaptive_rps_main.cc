// Command line adaptive RPS controller driving a Nighthawk Service.

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "common/protobuf/protobuf.h"
#include "file/base/file.h"
#include "file/base/helpers.h"
#include "file/base/options.h"
#include "gfe/tools/loadtesting/unified_load_test/adaptive_rps/adaptive_rps_controller.h"
#include "google/protobuf/wrappers.proto.h"
#include "google/rpc/status.proto.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "envoy/src/api/envoy/config/core/v3/base.proto.h"
#include "nighthawk/api/client/options.proto.h"
#include "nighthawk/api/client/output.proto.h"
#include "nighthawk/api/client/service.grpc.pb.h"
#include "nighthawk/api/client/service.proto.h"
#include "util/task/status.h"

ABSL_FLAG(std::string, api_server, "localhost:8443",
          "address of Nighthawk gRPC server (Nighthawk Traffic API) to send "
          "requests to");
ABSL_FLAG(std::string, spec_file, "",
          "A textproto specification of "
          "nighthawk::adaptive_rps::AdaptiveRpsSessionSpec");
ABSL_FLAG(std::string, output_file, "",
          "The path to write textproto output of type "
          "nighthawk::adaptive_rps::AdaptiveRpsSessionOutput");

int main(int argc, char* argv[]) {
  std::string input_filename = absl::GetFlag(FLAGS_spec_file);
  std::string input_textproto;
  CHECK_OK(file::GetContents(input_filename, &input_textproto, file::Defaults()));

  nighthawk::adaptive_rps::AdaptiveRpsSessionSpec spec =
      proto2::contrib::parse_proto::ParseTextProtoOrDie(input_textproto);

  std::shared_ptr<grpc_impl::ChannelCredentials> credentials;
  credentials = grpc::experimental::LocalCredentials(LOCAL_TCP);

  std::string api_server = absl::GetFlag(FLAGS_api_server);
  std::shared_ptr<grpc_impl::Channel> channel = grpc::CreateChannel(api_server, credentials);

  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub(
      nighthawk::client::NighthawkService::NewStub(channel));

  nighthawk::adaptive_rps::AdaptiveRpsSessionOutput output =
      nighthawk::PerformAdaptiveRpsSession(stub.get(), spec);

  std::string output_filename = absl::GetFlag(FLAGS_output_file);
  File* file = file::OpenOrDie(output_filename, "w", file::Defaults());
  CHECK_OK(file::WriteString(file, output.DebugString(), file::Defaults()));
  CHECK_OK(file->Close(file::Defaults()));

  return 0;
}
