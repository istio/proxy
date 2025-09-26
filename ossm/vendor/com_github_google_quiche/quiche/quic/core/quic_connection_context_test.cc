// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_connection_context.h"

#include <memory>
#include <string>
#include <vector>

#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/platform/api/quic_thread.h"

using testing::ElementsAre;

namespace quic::test {
namespace {

class TraceCollector : public QuicConnectionTracer {
 public:
  ~TraceCollector() override = default;

  void PrintLiteral(const char* literal) override { trace_.push_back(literal); }

  void PrintString(absl::string_view s) override {
    trace_.push_back(std::string(s));
  }

  const std::vector<std::string>& trace() const { return trace_; }

 private:
  std::vector<std::string> trace_;
};

struct FakeConnection {
  FakeConnection() { context.tracer = std::make_unique<TraceCollector>(); }

  const std::vector<std::string>& trace() const {
    return static_cast<const TraceCollector*>(context.tracer.get())->trace();
  }

  QuicConnectionContext context;
};

void SimpleSwitch() {
  FakeConnection connection;

  // These should be ignored since current context is nullptr.
  EXPECT_EQ(QuicConnectionContext::Current(), nullptr);
  QUIC_TRACELITERAL("before switch: literal");
  QUIC_TRACESTRING(std::string("before switch: string"));
  QUIC_TRACEPRINTF("%s: %s", "before switch", "printf");

  {
    QuicConnectionContextSwitcher switcher(&connection.context);
    QUIC_TRACELITERAL("literal");
    QUIC_TRACESTRING(std::string("string"));
    QUIC_TRACEPRINTF("%s", "printf");
  }

  EXPECT_EQ(QuicConnectionContext::Current(), nullptr);
  QUIC_TRACELITERAL("after switch: literal");
  QUIC_TRACESTRING(std::string("after switch: string"));
  QUIC_TRACEPRINTF("%s: %s", "after switch", "printf");

  EXPECT_THAT(connection.trace(), ElementsAre("literal", "string", "printf"));
}

void NestedSwitch() {
  FakeConnection outer, inner;

  {
    QuicConnectionContextSwitcher switcher(&outer.context);
    QUIC_TRACELITERAL("outer literal 0");
    QUIC_TRACESTRING(std::string("outer string 0"));
    QUIC_TRACEPRINTF("%s %s %d", "outer", "printf", 0);

    {
      QuicConnectionContextSwitcher nested_switcher(&inner.context);
      QUIC_TRACELITERAL("inner literal");
      QUIC_TRACESTRING(std::string("inner string"));
      QUIC_TRACEPRINTF("%s %s", "inner", "printf");
    }

    QUIC_TRACELITERAL("outer literal 1");
    QUIC_TRACESTRING(std::string("outer string 1"));
    QUIC_TRACEPRINTF("%s %s %d", "outer", "printf", 1);
  }

  EXPECT_THAT(outer.trace(), ElementsAre("outer literal 0", "outer string 0",
                                         "outer printf 0", "outer literal 1",
                                         "outer string 1", "outer printf 1"));

  EXPECT_THAT(inner.trace(),
              ElementsAre("inner literal", "inner string", "inner printf"));
}

void AlternatingSwitch() {
  FakeConnection zero, one, two;
  for (int i = 0; i < 15; ++i) {
    FakeConnection* connection =
        ((i % 3) == 0) ? &zero : (((i % 3) == 1) ? &one : &two);

    QuicConnectionContextSwitcher switcher(&connection->context);
    QUIC_TRACEPRINTF("%d", i);
  }

  EXPECT_THAT(zero.trace(), ElementsAre("0", "3", "6", "9", "12"));
  EXPECT_THAT(one.trace(), ElementsAre("1", "4", "7", "10", "13"));
  EXPECT_THAT(two.trace(), ElementsAre("2", "5", "8", "11", "14"));
}

typedef void (*ThreadFunction)();

template <ThreadFunction func>
class TestThread : public QuicThread {
 public:
  TestThread() : QuicThread("TestThread") {}
  ~TestThread() override = default;

 protected:
  void Run() override { func(); }
};

template <ThreadFunction func>
void RunInThreads(size_t n_threads) {
  using ThreadType = TestThread<func>;
  std::vector<ThreadType> threads(n_threads);

  for (ThreadType& t : threads) {
    t.Start();
  }

  for (ThreadType& t : threads) {
    t.Join();
  }
}

class QuicConnectionContextTest : public QuicTest {
 protected:
};

TEST_F(QuicConnectionContextTest, NullTracerOK) {
  FakeConnection connection;
  std::unique_ptr<QuicConnectionTracer> tracer;

  {
    QuicConnectionContextSwitcher switcher(&connection.context);
    QUIC_TRACELITERAL("msg 1 recorded");
  }

  connection.context.tracer.swap(tracer);

  {
    QuicConnectionContextSwitcher switcher(&connection.context);
    // Should be a no-op since connection.context.tracer is nullptr.
    QUIC_TRACELITERAL("msg 2 ignored");
  }

  EXPECT_THAT(static_cast<TraceCollector*>(tracer.get())->trace(),
              ElementsAre("msg 1 recorded"));
}

TEST_F(QuicConnectionContextTest, TestSimpleSwitch) {
  RunInThreads<SimpleSwitch>(10);
}

TEST_F(QuicConnectionContextTest, TestNestedSwitch) {
  RunInThreads<NestedSwitch>(10);
}

TEST_F(QuicConnectionContextTest, TestAlternatingSwitch) {
  RunInThreads<AlternatingSwitch>(10);
}

}  // namespace
}  // namespace quic::test
