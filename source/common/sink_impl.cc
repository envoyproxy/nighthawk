#include "common/sink_impl.h"

#include <array>
#include <filesystem>
#include <fstream>

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {

// TODO: security aspects of filename?
absl::Status FileSinkImpl::StoreExecutionResultPiece(
    const ::nighthawk::client::ExecutionResponse& response) const {
  const std::string& execution_id = response.execution_id();
  if (execution_id.empty()) {
    return absl::Status(absl::StatusCode::kInternal, "Received an empty execution id");
  }
  std::filesystem::create_directories("/tmp/nh/" + std::string(execution_id) + "/");
  std::array<char, L_tmpnam> name_buffer;
  // TODO(oschaaf): tmpname complaint from compiler, dangerous. Portable mkstemp?
  if (!std::tmpnam(name_buffer.data())) {
    return absl::Status(absl::StatusCode::kInternal, "Failure creating temp file");
  }
  std::ofstream ofs(name_buffer.data(), std::ios_base::out | std::ios_base::binary);
  if (!response.SerializeToOstream(&ofs)) {
    return absl::Status(absl::StatusCode::kNotFound, "Failure writing to temp file");
  }
  ofs.close();
  std::filesystem::path filesystem_path(name_buffer.data());
  try {
    const std::string new_name =
        "/tmp/nh/" + std::string(execution_id) + "/" + std::string(filesystem_path.filename());
    std::filesystem::rename(name_buffer.data(), new_name);
    ENVOY_LOG_MISC(trace, "Stored '{}'.", new_name);
  } catch (std::exception& ex) {
    return absl::Status(absl::StatusCode::kInternal, ex.what());
  }
  return absl::Status();
}

const absl::StatusOr<std::vector<::nighthawk::client::ExecutionResponse>>
FileSinkImpl::LoadExecutionResult(absl::string_view execution_id) const {
  if (execution_id.empty()) {
    return absl::Status(absl::StatusCode::kInternal, "Received an empty execution id");
  }
  std::filesystem::path filesystem_directory_path("/tmp/nh/" + std::string(execution_id) + "/");
  std::vector<::nighthawk::client::ExecutionResponse> responses;
  std::error_code ec;

  for (const auto& it : std::filesystem::directory_iterator(filesystem_directory_path, ec)) {
    ::nighthawk::client::ExecutionResponse response;
    std::ifstream ifs(it.path(), std::ios_base::binary);
    if (!response.ParseFromIstream(&ifs)) {
      return absl::Status(absl::StatusCode::kInternal,
                          fmt::format("Failure reading/parsing '{}'.", it.path()));
    } else {
      ENVOY_LOG_MISC(trace, "Loaded '{}'.", it.path());
    }
    responses.push_back(response);
  }
  if (ec.value()) {
    // TODO(oschaaf): could ec.message() contain sensitive information?
    return absl::Status(absl::StatusCode::kNotFound, ec.message());
  }
  return responses;
}

} // namespace Nighthawk
