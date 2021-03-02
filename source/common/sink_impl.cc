#include "common/sink_impl.h"

#include <array>
#include <filesystem>
#include <fstream>

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/random_generator.h"

namespace Nighthawk {
namespace {

absl::Status verifyCanBeUsedAsDirectoryName(absl::string_view s) {
  Envoy::Random::RandomGeneratorImpl random;
  const std::string reference_value = random.uuid();
  const std::string err_template = "'{}' is not a guid: {}";

  if (s.size() != reference_value.size()) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        fmt::format(err_template, s, "bad string length."));
  }
  for (size_t i = 0; i < s.size(); i++) {
    if (reference_value[i] == '-') {
      if (s[i] != '-') {
        return absl::Status(
            absl::StatusCode::kInvalidArgument,
            fmt::format(err_template, s, "expectations around '-' positions not met."));
      }
      continue;
    }
    if (!std::isxdigit(s[i])) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          fmt::format(err_template, s, "unexpected character encountered."));
    }
  }
  return absl::OkStatus();
}

} // namespace

absl::Status FileSinkImpl::StoreExecutionResultPiece(
    const ::nighthawk::client::ExecutionResponse& response) const {
  const std::string& execution_id = response.execution_id();
  absl::Status status = verifyCanBeUsedAsDirectoryName(execution_id);
  if (!status.ok()) {
    return status;
  }
  std::error_code error_code;
  std::filesystem::create_directories("/tmp/nh/" + std::string(execution_id) + "/", error_code);
  // Note error_code will not be set if an existing directory existed.
  if (error_code.value()) {
    return absl::Status(absl::StatusCode::kInternal, error_code.message());
  }
  // Write to a tmp file, and if that succeeds, we swap it atomically to the target path,
  // to make the completely written file visible to consumers of LoadExecutionResult.
  Envoy::Random::RandomGeneratorImpl random;
  const std::string uid = "/tmp/nighthawk_" + random.uuid();
  {
    std::ofstream ofs(uid.data(), std::ios_base::out | std::ios_base::binary);
    if (!response.SerializeToOstream(&ofs)) {
      return absl::Status(absl::StatusCode::kInternal, "Failure writing to temp file");
    }
  }
  std::filesystem::path filesystem_path(uid.data());
  const std::string new_name =
      "/tmp/nh/" + std::string(execution_id) + "/" + std::string(filesystem_path.filename());
  std::filesystem::rename(uid.data(), new_name, error_code);
  if (error_code.value()) {
    return absl::Status(absl::StatusCode::kInternal, error_code.message());
  }
  ENVOY_LOG_MISC(trace, "Stored '{}'.", new_name);
  return absl::Status();
}

absl::StatusOr<std::vector<::nighthawk::client::ExecutionResponse>>
FileSinkImpl::LoadExecutionResult(absl::string_view execution_id) const {
  absl::Status status = verifyCanBeUsedAsDirectoryName(execution_id);
  if (!status.ok()) {
    return status;
  }

  std::filesystem::path filesystem_directory_path("/tmp/nh/" + std::string(execution_id) + "/");
  std::vector<::nighthawk::client::ExecutionResponse> responses;
  std::error_code error_code;

  for (const auto& it :
       std::filesystem::directory_iterator(filesystem_directory_path, error_code)) {
    if (error_code.value()) {
      break;
    }
    ::nighthawk::client::ExecutionResponse response;
    std::ifstream ifs(it.path(), std::ios_base::binary);
    if (!response.ParseFromIstream(&ifs)) {
      return absl::Status(absl::StatusCode::kInternal,
                          fmt::format("Failed to parse ExecutionResponse '{}'.", it.path()));
    } else {
      ENVOY_LOG_MISC(trace, "Loaded '{}'.", it.path());
    }
    responses.push_back(response);
  }
  if (error_code.value()) {
    return absl::Status(absl::StatusCode::kNotFound, error_code.message());
  }
  return responses;
}

} // namespace Nighthawk
