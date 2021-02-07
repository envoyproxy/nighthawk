#include "common/sink_impl.h"

#include <array>
#include <filesystem>
#include <fstream>

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/random_generator.h"

namespace Nighthawk {
namespace {

absl::Status verifyStringIsGuid(absl::string_view s) {
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
  absl::Status status = verifyStringIsGuid(execution_id);
  if (!status.ok()) {
    return status;
  }
  std::filesystem::create_directories("/tmp/nh/" + std::string(execution_id) + "/");
  std::array<char, L_tmpnam> name_buffer;
  // TODO(oschaaf): tmpname complaint from compiler, dangerous.
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
  absl::Status status = verifyStringIsGuid(execution_id);
  if (!status.ok()) {
    return status; //
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
