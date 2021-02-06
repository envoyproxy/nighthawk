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
    ENVOY_LOG_MISC(error, "Failure creating temp file '{}'", name_buffer.data());
    return absl::Status(absl::StatusCode::kInternal, "Failure creating temp file");
  }

  std::ofstream ofs(name_buffer.data(), std::ios_base::out | std::ios_base::binary);
  if (!response.SerializeToOstream(&ofs)) {
    ENVOY_LOG_MISC(error, "Failure writing to tmp file '{}'", name_buffer.data());
    return absl::Status(absl::StatusCode::kNotFound, "Failure writing to temp file");
  }
  ofs.close();
  std::filesystem::path filesystem_path(name_buffer.data());
  try {
    const std::string new_name =
        "/tmp/nh/" + std::string(execution_id) + "/" + std::string(filesystem_path.filename());
    std::filesystem::rename(name_buffer.data(), new_name);
    ENVOY_LOG_MISC(info, "Stored '{}'.", new_name);
  } catch (std::exception& ex) {
    ENVOY_LOG_MISC(error, "Failure renaming temp file '{}': {}", name_buffer.data(), ex.what());
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
  ENVOY_LOG_MISC(error, "Sink loading results from '{}'", filesystem_directory_path);

  std::error_code ec;
  for (const auto& it : std::filesystem::directory_iterator(filesystem_directory_path, ec)) {
    ::nighthawk::client::ExecutionResponse response;
    std::ifstream ifs(it.path(), std::ios_base::binary);
    if (!response.ParseFromIstream(&ifs)) {
      ENVOY_LOG_MISC(error, "Failure reading/parsing '{}'.", it.path());
      return absl::Status(absl::StatusCode::kInternal,
                          fmt::format("Failure reading/parsing '{}'.", it.path()));
    } else {
      ENVOY_LOG_MISC(info, "Loaded '{}'.", it.path());
    }
    responses.push_back(response);
  }
  if (ec.value()) {
    // TODO(oschaaf): could this have sensitive information?
    ENVOY_LOG_MISC(error, "Failure iterating '{}': {}", filesystem_directory_path, ec.message());
    return absl::Status(absl::StatusCode::kNotFound, ec.message());
  }
  return responses;
}

} // namespace Nighthawk
