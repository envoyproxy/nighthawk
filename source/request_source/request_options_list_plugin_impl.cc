#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"
#include "external/envoy/source/exe/platform_impl.h"

#include "api/client/options.pb.h"

#include "common/request_impl.h"
#include "common/request_source_impl.h"

#include "request_source/request_source_plugin_impl.h"

namespace Nighthawk {
std::string FileBasedRequestSourcePluginConfigFactory::name() const {
  return "nighthawk.file-based-request-source-plugin";
}

Envoy::ProtobufTypes::MessagePtr
FileBasedRequestSourcePluginConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::request_source::FileBasedPluginConfig>();
}

RequestSourcePtr FileBasedRequestSourcePluginConfigFactory::createRequestSourcePlugin(
    const Envoy::Protobuf::Message& message, Envoy::Api::ApiPtr api,
    Envoy::Http::RequestHeaderMapPtr header) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::FileBasedPluginConfig config;
  Envoy::MessageUtil util;

  util.unpackTo(any, config);
  if (api->fileSystem().fileSize(config.file_path()) > config.max_file_size().value()) {
    throw NighthawkException("file size must be less than max_file_size");
  }
  auto temp_list = std::make_unique<nighthawk::client::RequestOptionsList>();

  // Locking to avoid issues with multiple threads reading the same file.
  {
    Envoy::Thread::LockGuard lock_guard(file_lock_);
    // Reading the file only the first time.
    if (options_list_.options_size() == 0) {
      util.loadFromFile(config.file_path(), options_list_,
                        Envoy::ProtobufMessage::getStrictValidationVisitor(), *api, true);
    }
    temp_list->CopyFrom(options_list_);
  }
  return std::make_unique<RequestOptionsListRequestSource>(config.num_requests().value(),
                                                           std::move(header), std::move(temp_list));
}

REGISTER_FACTORY(FileBasedRequestSourcePluginConfigFactory, RequestSourcePluginConfigFactory);

RequestOptionsListRequestSource::RequestOptionsListRequestSource(
    const uint32_t request_max, Envoy::Http::RequestHeaderMapPtr header,
    std::unique_ptr<nighthawk::client::RequestOptionsList> options_list)
    : header_(std::move(header)), options_list_(std::move(options_list)),
      request_max_(request_max) {}

RequestGenerator RequestOptionsListRequestSource::get() {
  uint32_t counter = 0;
  request_count_.push_back(counter);
  uint32_t& lambda_counter = request_count_.back();
  RequestGenerator request_generator = [this, lambda_counter]() mutable -> RequestPtr {
    // if request_max is 0, then we never stop generating requests.
    if (lambda_counter >= request_max_ && request_max_ != 0) {
      return nullptr;
    }

    // Increment the counter and get the request_option from the list for the current iteration.
    unsigned int index = lambda_counter % options_list_->options_size();
    nighthawk::client::RequestOptions request_option = options_list_->options().at(index);
    ++lambda_counter;

    // Initialize the header with the values from the default header.
    Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
    Envoy::Http::HeaderMapImpl::copyFrom(*header, *header_);

    // Override the default values with the values from the request_option
    header->setMethod(envoy::config::core::v3::RequestMethod_Name(request_option.request_method()));
    const uint32_t content_length = request_option.request_body_size().value();
    if (content_length > 0) {
      header->setContentLength(content_length);
    }
    for (const envoy::config::core::v3::HeaderValueOption& option_header :
         request_option.request_headers()) {
      auto lower_case_key = Envoy::Http::LowerCaseString(std::string(option_header.header().key()));
      header->setCopy(lower_case_key, std::string(option_header.header().value()));
    }
    return std::make_unique<RequestImpl>(std::move(header));
  };
  return request_generator;
}

void RequestOptionsListRequestSource::initOnThread() {}

} // namespace Nighthawk