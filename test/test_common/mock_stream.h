#pragma once

#include <gmock/gmock.h>
#include <grpcpp/support/sync_stream.h>

namespace Nighthawk {

// Based off of "grpcpp/test/mock_stream.h"
template <class W> class MockClientWriter : public grpc::ClientWriterInterface<W> {
public:
  MockClientWriter() = default;

  /// ClientStreamingInterface
  MOCK_METHOD0_T(Finish, grpc::Status());

  /// WriterInterface
  MOCK_METHOD2_T(Write, bool(const W&, const grpc::WriteOptions));

  /// ClientWriterInterface
  MOCK_METHOD0_T(WritesDone, bool());
};

// Based off of "grpcpp/test/mock_stream.h"
template <class W, class R>
class MockClientReaderWriter : public grpc::ClientReaderWriterInterface<W, R> {
public:
  MockClientReaderWriter() = default;

  /// ClientStreamingInterface
  MOCK_METHOD0_T(Finish, grpc::Status());

  /// ReaderInterface
  MOCK_METHOD1_T(NextMessageSize, bool(uint32_t*));
  MOCK_METHOD1_T(Read, bool(R*));

  /// WriterInterface
  MOCK_METHOD2_T(Write, bool(const W&, const grpc::WriteOptions));

  /// ClientReaderWriterInterface
  MOCK_METHOD0_T(WaitForInitialMetadata, void());
  MOCK_METHOD0_T(WritesDone, bool());
};

} // namespace Nighthawk
