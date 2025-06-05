// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/web_transport/encapsulated/encapsulated_web_transport.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/common/capsule.h"
#include "quiche/common/http/http_header_block.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/quiche_stream.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/mock_streams.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

namespace webtransport::test {
namespace {

using ::quiche::Capsule;
using ::quiche::CapsuleType;
using ::quiche::test::StatusIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::StrEq;

class EncapsulatedWebTransportTest : public quiche::test::QuicheTest,
                                     public quiche::CapsuleParser::Visitor {
 public:
  EncapsulatedWebTransportTest() : parser_(this), reader_(&read_buffer_) {
    ON_CALL(fatal_error_callback_, Call(_))
        .WillByDefault([](absl::string_view error) {
          ADD_FAILURE() << "Fatal session error: " << error;
        });
    ON_CALL(writer_, Writev(_, _))
        .WillByDefault([&](absl::Span<const absl::string_view> data,
                           const quiche::StreamWriteOptions& options) {
          for (absl::string_view fragment : data) {
            parser_.IngestCapsuleFragment(fragment);
          }
          writer_.ProcessOptions(options);
          return absl::OkStatus();
        });
  }

  std::unique_ptr<EncapsulatedSession> CreateTransport(
      Perspective perspective) {
    auto transport = std::make_unique<EncapsulatedSession>(
        perspective, fatal_error_callback_.AsStdFunction());
    session_ = transport.get();
    return transport;
  }

  std::unique_ptr<SessionVisitor> CreateAndStoreVisitor() {
    auto visitor = std::make_unique<testing::StrictMock<MockSessionVisitor>>();
    visitor_ = visitor.get();
    return visitor;
  }

  MOCK_METHOD(bool, OnCapsule, (const Capsule&), (override));

  void OnCapsuleParseFailure(absl::string_view error_message) override {
    ADD_FAILURE() << "Written an invalid capsule: " << error_message;
  }

  void ProcessIncomingCapsule(const Capsule& capsule) {
    quiche::QuicheBuffer buffer =
        quiche::SerializeCapsule(capsule, quiche::SimpleBufferAllocator::Get());
    read_buffer_.append(buffer.data(), buffer.size());
    session_->OnCanRead();
  }

  template <typename CapsuleType>
  void ProcessIncomingCapsule(const CapsuleType& capsule) {
    quiche::QuicheBuffer buffer = quiche::SerializeCapsule(
        quiche::Capsule(capsule), quiche::SimpleBufferAllocator::Get());
    read_buffer_.append(buffer.data(), buffer.size());
    session_->OnCanRead();
  }

  void DefaultHandshakeForClient(EncapsulatedSession& session) {
    quiche::HttpHeaderBlock outgoing_headers, incoming_headers;
    session.InitializeClient(CreateAndStoreVisitor(), outgoing_headers,
                             &writer_, &reader_);
    EXPECT_CALL(*visitor_, OnSessionReady());
    session.ProcessIncomingServerHeaders(incoming_headers);
  }

 protected:
  quiche::CapsuleParser parser_;
  quiche::test::MockWriteStream writer_;
  std::string read_buffer_;
  quiche::test::ReadStreamFromString reader_;
  MockSessionVisitor* visitor_ = nullptr;
  EncapsulatedSession* session_ = nullptr;
  testing::MockFunction<void(absl::string_view)> fatal_error_callback_;
};

TEST_F(EncapsulatedWebTransportTest, IsOpenedBy) {
  EXPECT_EQ(IsIdOpenedBy(0x00, Perspective::kClient), true);
  EXPECT_EQ(IsIdOpenedBy(0x01, Perspective::kClient), false);
  EXPECT_EQ(IsIdOpenedBy(0x02, Perspective::kClient), true);
  EXPECT_EQ(IsIdOpenedBy(0x03, Perspective::kClient), false);

  EXPECT_EQ(IsIdOpenedBy(0x00, Perspective::kServer), false);
  EXPECT_EQ(IsIdOpenedBy(0x01, Perspective::kServer), true);
  EXPECT_EQ(IsIdOpenedBy(0x02, Perspective::kServer), false);
  EXPECT_EQ(IsIdOpenedBy(0x03, Perspective::kServer), true);
}

TEST_F(EncapsulatedWebTransportTest, SetupClientSession) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  quiche::HttpHeaderBlock outgoing_headers, incoming_headers;
  EXPECT_EQ(session->state(), EncapsulatedSession::kUninitialized);
  session->InitializeClient(CreateAndStoreVisitor(), outgoing_headers, &writer_,
                            &reader_);
  EXPECT_EQ(session->state(), EncapsulatedSession::kWaitingForHeaders);
  EXPECT_CALL(*visitor_, OnSessionReady());
  session->ProcessIncomingServerHeaders(incoming_headers);
  EXPECT_EQ(session->state(), EncapsulatedSession::kSessionOpen);
}

TEST_F(EncapsulatedWebTransportTest, SetupServerSession) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kServer);
  quiche::HttpHeaderBlock outgoing_headers, incoming_headers;
  EXPECT_EQ(session->state(), EncapsulatedSession::kUninitialized);
  std::unique_ptr<SessionVisitor> visitor = CreateAndStoreVisitor();
  EXPECT_CALL(*visitor_, OnSessionReady());
  session->InitializeServer(std::move(visitor), outgoing_headers,
                            incoming_headers, &writer_, &reader_);
  EXPECT_EQ(session->state(), EncapsulatedSession::kSessionOpen);
}

TEST_F(EncapsulatedWebTransportTest, CloseSession) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::CLOSE_WEBTRANSPORT_SESSION);
    EXPECT_EQ(capsule.close_web_transport_session_capsule().error_code, 0x1234);
    EXPECT_EQ(capsule.close_web_transport_session_capsule().error_message,
              "test close");
    return true;
  });
  EXPECT_EQ(session->state(), EncapsulatedSession::kSessionOpen);
  EXPECT_CALL(*visitor_, OnSessionClosed(0x1234, StrEq("test close")));
  session->CloseSession(0x1234, "test close");
  EXPECT_EQ(session->state(), EncapsulatedSession::kSessionClosed);
  EXPECT_TRUE(writer_.fin_written());

  EXPECT_CALL(fatal_error_callback_, Call(_))
      .WillOnce([](absl::string_view error) {
        EXPECT_THAT(error, HasSubstr("close a session that is already closed"));
      });
  session->CloseSession(0x1234, "test close");
}

TEST_F(EncapsulatedWebTransportTest, CloseSessionWriteBlocked) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(writer_, CanWrite()).WillOnce(Return(false));
  EXPECT_CALL(*this, OnCapsule(_)).Times(0);
  EXPECT_EQ(session->state(), EncapsulatedSession::kSessionOpen);
  session->CloseSession(0x1234, "test close");
  EXPECT_EQ(session->state(), EncapsulatedSession::kSessionClosing);

  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::CLOSE_WEBTRANSPORT_SESSION);
    EXPECT_EQ(capsule.close_web_transport_session_capsule().error_code, 0x1234);
    EXPECT_EQ(capsule.close_web_transport_session_capsule().error_message,
              "test close");
    return true;
  });
  EXPECT_CALL(writer_, CanWrite()).WillOnce(Return(true));
  EXPECT_CALL(*visitor_, OnSessionClosed(0x1234, StrEq("test close")));
  session->OnCanWrite();
  EXPECT_EQ(session->state(), EncapsulatedSession::kSessionClosed);
  EXPECT_TRUE(writer_.fin_written());
}

TEST_F(EncapsulatedWebTransportTest, ReceiveFin) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);

  EXPECT_CALL(*visitor_, OnSessionClosed(0, IsEmpty()));
  reader_.set_fin();
  session->OnCanRead();
  EXPECT_TRUE(writer_.fin_written());
}

TEST_F(EncapsulatedWebTransportTest, ReceiveCloseSession) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);

  EXPECT_CALL(*visitor_, OnSessionClosed(0x1234, StrEq("test")));
  ProcessIncomingCapsule(Capsule::CloseWebTransportSession(0x1234, "test"));
  EXPECT_TRUE(writer_.fin_written());
  reader_.set_fin();
  session->OnCanRead();
}

TEST_F(EncapsulatedWebTransportTest, ReceiveMalformedData) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);

  EXPECT_CALL(fatal_error_callback_, Call(HasSubstr("too much capsule data")))
      .WillOnce([] {});
  read_buffer_ = std::string(2 * 1024 * 1024, '\xff');
  session->OnCanRead();
}

TEST_F(EncapsulatedWebTransportTest, SendDatagrams) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), quiche::CapsuleType::DATAGRAM);
    EXPECT_EQ(capsule.datagram_capsule().http_datagram_payload, "test");
    return true;
  });
  DatagramStatus status = session->SendOrQueueDatagram("test");
  EXPECT_EQ(status.code, DatagramStatusCode::kSuccess);
}

TEST_F(EncapsulatedWebTransportTest, SendDatagramsEarly) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  quiche::HttpHeaderBlock outgoing_headers;
  session->InitializeClient(CreateAndStoreVisitor(), outgoing_headers, &writer_,
                            &reader_);
  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), quiche::CapsuleType::DATAGRAM);
    EXPECT_EQ(capsule.datagram_capsule().http_datagram_payload, "test");
    return true;
  });
  ASSERT_EQ(session->state(), EncapsulatedSession::kWaitingForHeaders);
  session->SendOrQueueDatagram("test");
}

TEST_F(EncapsulatedWebTransportTest, SendDatagramsBeforeInitialization) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  quiche::HttpHeaderBlock outgoing_headers;
  EXPECT_CALL(*this, OnCapsule(_)).Times(0);
  ASSERT_EQ(session->state(), EncapsulatedSession::kUninitialized);
  session->SendOrQueueDatagram("test");

  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::DATAGRAM);
    EXPECT_EQ(capsule.datagram_capsule().http_datagram_payload, "test");
    return true;
  });
  DefaultHandshakeForClient(*session);
}

TEST_F(EncapsulatedWebTransportTest, SendDatagramsTooBig) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*this, OnCapsule(_)).Times(0);
  std::string long_string(16 * 1024, 'a');
  DatagramStatus status = session->SendOrQueueDatagram(long_string);
  EXPECT_EQ(status.code, DatagramStatusCode::kTooBig);
}

TEST_F(EncapsulatedWebTransportTest, ReceiveDatagrams) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnDatagramReceived(_))
      .WillOnce([](absl::string_view data) { EXPECT_EQ(data, "test"); });
  ProcessIncomingCapsule(Capsule::Datagram("test"));
}

TEST_F(EncapsulatedWebTransportTest, SendDraining) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::DRAIN_WEBTRANSPORT_SESSION);
    return true;
  });
  session->NotifySessionDraining();
}

TEST_F(EncapsulatedWebTransportTest, ReceiveDraining) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  testing::MockFunction<void()> callback;
  session->SetOnDraining(callback.AsStdFunction());
  EXPECT_CALL(callback, Call());
  ProcessIncomingCapsule(Capsule(quiche::DrainWebTransportSessionCapsule()));
}

TEST_F(EncapsulatedWebTransportTest, WriteErrorDatagram) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(writer_, Writev(_, _))
      .WillOnce(Return(absl::InternalError("Test write error")));
  EXPECT_CALL(fatal_error_callback_, Call(_))
      .WillOnce([](absl::string_view error) {
        EXPECT_THAT(error, HasSubstr("Test write error"));
      });
  DatagramStatus status = session->SendOrQueueDatagram("test");
  EXPECT_EQ(status.code, DatagramStatusCode::kInternalError);
}

TEST_F(EncapsulatedWebTransportTest, WriteErrorControlCapsule) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(writer_, Writev(_, _))
      .WillOnce(Return(absl::InternalError("Test write error")));
  EXPECT_CALL(fatal_error_callback_, Call(_))
      .WillOnce([](absl::string_view error) {
        EXPECT_THAT(error, HasSubstr("Test write error"));
      });
  session->NotifySessionDraining();
}

TEST_F(EncapsulatedWebTransportTest, SimpleRead) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  bool stream_received = false;
  EXPECT_CALL(*visitor_, OnIncomingBidirectionalStreamAvailable())
      .WillOnce([&] { stream_received = true; });
  std::string data = "test";
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{1, data, false});
  // Make sure data gets copied.
  data[0] = 'q';
  EXPECT_TRUE(stream_received);
  Stream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(stream->GetStreamId(), 1u);
  EXPECT_EQ(stream->visitor(), nullptr);
  EXPECT_EQ(stream->ReadableBytes(), 4u);

  quiche::ReadStream::PeekResult peek = stream->PeekNextReadableRegion();
  EXPECT_EQ(peek.peeked_data, "test");
  EXPECT_FALSE(peek.fin_next);
  EXPECT_FALSE(peek.all_data_received);

  std::string buffer;
  quiche::ReadStream::ReadResult read = stream->Read(&buffer);
  EXPECT_EQ(read.bytes_read, 4);
  EXPECT_FALSE(read.fin);
  EXPECT_EQ(buffer, "test");
  EXPECT_EQ(stream->ReadableBytes(), 0u);
}

class MockStreamVisitorWithDestructor : public MockStreamVisitor {
 public:
  ~MockStreamVisitorWithDestructor() { OnDelete(); }

  MOCK_METHOD(void, OnDelete, (), ());
};

MockStreamVisitorWithDestructor* SetupVisitor(Stream& stream) {
  auto visitor = std::make_unique<MockStreamVisitorWithDestructor>();
  MockStreamVisitorWithDestructor* result = visitor.get();
  stream.SetVisitor(std::move(visitor));
  return result;
}

TEST_F(EncapsulatedWebTransportTest, ImmediateRead) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingBidirectionalStreamAvailable());
  ProcessIncomingCapsule(
      quiche::WebTransportStreamDataCapsule{1, "abcd", false});
  Stream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(stream->ReadableBytes(), 4u);

  MockStreamVisitor* visitor = SetupVisitor(*stream);
  EXPECT_CALL(*visitor, OnCanRead()).WillOnce([&] {
    std::string output;
    (void)stream->Read(&output);
    EXPECT_EQ(output, "abcdef");
  });
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{1, "ef", false});
}

TEST_F(EncapsulatedWebTransportTest, FinPeek) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingBidirectionalStreamAvailable());
  ProcessIncomingCapsule(
      quiche::WebTransportStreamDataCapsule{1, "abcd", false});
  Stream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(stream->ReadableBytes(), 4u);

  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{1, "ef", true});

  quiche::ReadStream::PeekResult peek = stream->PeekNextReadableRegion();
  EXPECT_EQ(peek.peeked_data, "abcd");
  EXPECT_FALSE(peek.fin_next);
  EXPECT_TRUE(peek.all_data_received);

  EXPECT_FALSE(stream->SkipBytes(2));
  peek = stream->PeekNextReadableRegion();
  EXPECT_FALSE(peek.fin_next);
  EXPECT_TRUE(peek.all_data_received);

  EXPECT_FALSE(stream->SkipBytes(2));
  peek = stream->PeekNextReadableRegion();
  EXPECT_EQ(peek.peeked_data, "ef");
  EXPECT_TRUE(peek.fin_next);
  EXPECT_TRUE(peek.all_data_received);

  EXPECT_TRUE(stream->SkipBytes(2));
}

TEST_F(EncapsulatedWebTransportTest, FinRead) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingBidirectionalStreamAvailable());
  ProcessIncomingCapsule(
      quiche::WebTransportStreamDataCapsule{1, "abcdef", true});
  Stream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(stream->ReadableBytes(), 6u);

  std::array<char, 3> buffer;
  quiche::ReadStream::ReadResult read = stream->Read(absl::MakeSpan(buffer));
  EXPECT_THAT(buffer, ElementsAre('a', 'b', 'c'));
  EXPECT_EQ(read.bytes_read, 3);
  EXPECT_FALSE(read.fin);

  read = stream->Read(absl::MakeSpan(buffer));
  EXPECT_THAT(buffer, ElementsAre('d', 'e', 'f'));
  EXPECT_EQ(read.bytes_read, 3);
  EXPECT_TRUE(read.fin);
}

TEST_F(EncapsulatedWebTransportTest, LargeRead) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingBidirectionalStreamAvailable());
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{
      1, std::string(64 * 1024, 'a'), true});
  Stream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(stream->ReadableBytes(), 65536u);

  for (int i = 0; i < 64; i++) {
    std::array<char, 1024> buffer;
    quiche::ReadStream::ReadResult read = stream->Read(absl::MakeSpan(buffer));
    EXPECT_EQ(read.bytes_read, 1024);
    EXPECT_EQ(read.fin, i == 63);
  }
}

TEST_F(EncapsulatedWebTransportTest, DoubleFinReceived) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingBidirectionalStreamAvailable());
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{1, "abc", true});
  Stream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  EXPECT_CALL(fatal_error_callback_, Call(_))
      .WillOnce([](absl::string_view error) {
        EXPECT_THAT(error, HasSubstr("has already received a FIN"));
      });
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{1, "def", true});
}

TEST_F(EncapsulatedWebTransportTest, CanWriteUnidiBidi) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingBidirectionalStreamAvailable());
  EXPECT_CALL(*visitor_, OnIncomingUnidirectionalStreamAvailable());
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{1, "abc", true});
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{3, "abc", true});

  Stream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_TRUE(stream->CanWrite());

  stream = session->AcceptIncomingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_FALSE(stream->CanWrite());

  stream = session->OpenOutgoingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_TRUE(stream->CanWrite());

  stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_TRUE(stream->CanWrite());
}

TEST_F(EncapsulatedWebTransportTest, ReadOnlyGarbageCollection) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingUnidirectionalStreamAvailable());
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{3, "abc", true});

  Stream* stream = session->AcceptIncomingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_TRUE(stream->SkipBytes(3));

  MockStreamVisitorWithDestructor* visitor = SetupVisitor(*stream);
  bool deleted = false;
  EXPECT_CALL(*visitor, OnDelete()).WillOnce([&] { deleted = true; });
  session->GarbageCollectStreams();
  EXPECT_TRUE(deleted);
}

TEST_F(EncapsulatedWebTransportTest, WriteOnlyGarbageCollection) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);

  Stream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  MockStreamVisitorWithDestructor* visitor = SetupVisitor(*stream);
  bool deleted = false;
  EXPECT_CALL(*visitor, OnDelete()).WillOnce([&] { deleted = true; });
  EXPECT_CALL(*this, OnCapsule(_)).WillOnce(Return(true));

  quiche::StreamWriteOptions options;
  options.set_send_fin(true);
  EXPECT_THAT(stream->Writev(absl::Span<const absl::string_view>(), options),
              StatusIs(absl::StatusCode::kOk));
  session->GarbageCollectStreams();
  EXPECT_TRUE(deleted);
}

TEST_F(EncapsulatedWebTransportTest, SimpleWrite) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingBidirectionalStreamAvailable());
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{1, "", true});
  Stream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::WT_STREAM);
    EXPECT_EQ(capsule.web_transport_stream_data().stream_id, 1u);
    EXPECT_EQ(capsule.web_transport_stream_data().fin, false);
    EXPECT_EQ(capsule.web_transport_stream_data().data, "test");
    return true;
  });
  absl::Status status = quiche::WriteIntoStream(*stream, "test");
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));
}

TEST_F(EncapsulatedWebTransportTest, WriteWithFin) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  Stream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::WT_STREAM_WITH_FIN);
    EXPECT_EQ(capsule.web_transport_stream_data().stream_id, 2u);
    EXPECT_EQ(capsule.web_transport_stream_data().fin, true);
    EXPECT_EQ(capsule.web_transport_stream_data().data, "test");
    return true;
  });
  quiche::StreamWriteOptions options;
  options.set_send_fin(true);
  EXPECT_TRUE(stream->CanWrite());
  absl::Status status = quiche::WriteIntoStream(*stream, "test", options);
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));
  EXPECT_FALSE(stream->CanWrite());
}

TEST_F(EncapsulatedWebTransportTest, FinOnlyWrite) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  Stream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::WT_STREAM_WITH_FIN);
    EXPECT_EQ(capsule.web_transport_stream_data().stream_id, 2u);
    EXPECT_EQ(capsule.web_transport_stream_data().fin, true);
    EXPECT_EQ(capsule.web_transport_stream_data().data, "");
    return true;
  });
  quiche::StreamWriteOptions options;
  options.set_send_fin(true);
  EXPECT_TRUE(stream->CanWrite());
  absl::Status status =
      stream->Writev(absl::Span<const absl::string_view>(), options);
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));
  EXPECT_FALSE(stream->CanWrite());
}

TEST_F(EncapsulatedWebTransportTest, BufferedWriteThenUnbuffer) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  Stream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  EXPECT_CALL(writer_, CanWrite()).WillOnce(Return(false));
  absl::Status status = quiche::WriteIntoStream(*stream, "abc");
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));

  // While the stream cannot be written right now, we should be still able to
  // buffer data into it.
  EXPECT_TRUE(stream->CanWrite());
  EXPECT_CALL(writer_, CanWrite()).WillRepeatedly(Return(true));
  status = quiche::WriteIntoStream(*stream, "def");
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));

  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::WT_STREAM);
    EXPECT_EQ(capsule.web_transport_stream_data().stream_id, 2u);
    EXPECT_EQ(capsule.web_transport_stream_data().data, "abcdef");
    return true;
  });
  session_->OnCanWrite();
}

TEST_F(EncapsulatedWebTransportTest, BufferedWriteThenFlush) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  Stream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  EXPECT_CALL(writer_, CanWrite()).Times(2).WillRepeatedly(Return(false));
  absl::Status status = quiche::WriteIntoStream(*stream, "abc");
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));
  status = quiche::WriteIntoStream(*stream, "def");
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));

  EXPECT_CALL(writer_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::WT_STREAM);
    EXPECT_EQ(capsule.web_transport_stream_data().stream_id, 2u);
    EXPECT_EQ(capsule.web_transport_stream_data().data, "abcdef");
    return true;
  });
  session_->OnCanWrite();
}

TEST_F(EncapsulatedWebTransportTest, BufferedStreamBlocksAnother) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  Stream* stream1 = session->OpenOutgoingUnidirectionalStream();
  Stream* stream2 = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream1 != nullptr);
  ASSERT_TRUE(stream2 != nullptr);

  EXPECT_CALL(*this, OnCapsule(_)).Times(0);
  EXPECT_CALL(writer_, CanWrite()).WillOnce(Return(false));
  absl::Status status = quiche::WriteIntoStream(*stream1, "abc");
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));
  // ShouldYield will return false here, causing the write to get buffered.
  EXPECT_CALL(writer_, CanWrite()).WillRepeatedly(Return(true));
  status = quiche::WriteIntoStream(*stream2, "abc");
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kOk));

  std::vector<StreamId> writes;
  EXPECT_CALL(*this, OnCapsule(_)).WillRepeatedly([&](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::WT_STREAM);
    writes.push_back(capsule.web_transport_stream_data().stream_id);
    return true;
  });
  session_->OnCanWrite();
  EXPECT_THAT(writes, ElementsAre(2, 6));
}

TEST_F(EncapsulatedWebTransportTest, SendReset) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  Stream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  MockStreamVisitorWithDestructor* visitor = SetupVisitor(*stream);
  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([&](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::WT_RESET_STREAM);
    EXPECT_EQ(capsule.web_transport_reset_stream().stream_id, 2u);
    EXPECT_EQ(capsule.web_transport_reset_stream().error_code, 1234u);
    return true;
  });
  stream->ResetWithUserCode(1234u);

  bool deleted = false;
  EXPECT_CALL(*visitor, OnDelete()).WillOnce([&] { deleted = true; });
  session->GarbageCollectStreams();
  EXPECT_TRUE(deleted);
}

TEST_F(EncapsulatedWebTransportTest, ReceiveReset) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingUnidirectionalStreamAvailable());
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{3, "", true});
  Stream* stream = session->AcceptIncomingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  MockStreamVisitorWithDestructor* visitor = SetupVisitor(*stream);
  EXPECT_CALL(*visitor, OnResetStreamReceived(1234u));
  EXPECT_TRUE(session->GetStreamById(3) != nullptr);
  ProcessIncomingCapsule(quiche::WebTransportResetStreamCapsule{3u, 1234u});
  // Reading from the underlying transport automatically triggers garbage
  // collection.
  EXPECT_TRUE(session->GetStreamById(3) == nullptr);
}

TEST_F(EncapsulatedWebTransportTest, SendStopSending) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  EXPECT_CALL(*visitor_, OnIncomingUnidirectionalStreamAvailable());
  ProcessIncomingCapsule(quiche::WebTransportStreamDataCapsule{3, "", true});
  Stream* stream = session->AcceptIncomingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  MockStreamVisitorWithDestructor* visitor = SetupVisitor(*stream);
  EXPECT_CALL(*this, OnCapsule(_)).WillOnce([&](const Capsule& capsule) {
    EXPECT_EQ(capsule.capsule_type(), CapsuleType::WT_STOP_SENDING);
    EXPECT_EQ(capsule.web_transport_stop_sending().stream_id, 3u);
    EXPECT_EQ(capsule.web_transport_stop_sending().error_code, 1234u);
    return true;
  });
  stream->SendStopSending(1234u);

  bool deleted = false;
  EXPECT_CALL(*visitor, OnDelete()).WillOnce([&] { deleted = true; });
  session->GarbageCollectStreams();
  EXPECT_TRUE(deleted);
}

TEST_F(EncapsulatedWebTransportTest, ReceiveStopSending) {
  std::unique_ptr<EncapsulatedSession> session =
      CreateTransport(Perspective::kClient);
  DefaultHandshakeForClient(*session);
  Stream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  MockStreamVisitorWithDestructor* visitor = SetupVisitor(*stream);
  EXPECT_CALL(*visitor, OnStopSendingReceived(1234u));
  EXPECT_TRUE(session->GetStreamById(2) != nullptr);
  ProcessIncomingCapsule(quiche::WebTransportStopSendingCapsule{2u, 1234u});
  // Reading from the underlying transport automatically triggers garbage
  // collection.
  EXPECT_TRUE(session->GetStreamById(2) == nullptr);
}

}  // namespace
}  // namespace webtransport::test
