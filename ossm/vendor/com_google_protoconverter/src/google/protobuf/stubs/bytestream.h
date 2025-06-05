/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This file declares the ByteSink and ByteSource abstract interfaces. These
// interfaces represent objects that consume (ByteSink) or produce (ByteSource)
// a sequence of bytes. Using these abstract interfaces in your APIs can help
// make your code work with a variety of input and output types.
//
// This file also declares the following commonly used implementations of these
// interfaces.
//
//   ByteSink:
//      UncheckedArrayByteSink  Writes to an array, without bounds checking
//      CheckedArrayByteSink    Writes to an array, with bounds checking
//      GrowingArrayByteSink    Allocates and writes to a growable buffer
//      StringByteSink          Writes to an STL string
//      NullByteSink            Consumes a never-ending stream of bytes
//
//   ByteSource:
//      ArrayByteSource         Reads from an array or string
//      LimitedByteSource       Limits the number of bytes read from an

#ifndef GOOGLE_PROTOBUF_STUBS_BYTESTREAM_H_
#define GOOGLE_PROTOBUF_STUBS_BYTESTREAM_H_

#include <stddef.h>

#include <string>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/strings/string_view.h"

class CordByteSink;

namespace google {
namespace protobuf {
namespace strings {

// An abstract interface for an object that consumes a sequence of bytes. This
// interface offers a way to append data as well as a Flush() function.
//
// Example:
//
//   string my_data;
//   ...
//   ByteSink* sink = ...
//   sink->Append(my_data.data(), my_data.size());
//   sink->Flush();
//
class ByteSink {
 public:
  ByteSink() {}
  ByteSink(const ByteSink&) = delete;
  ByteSink& operator=(const ByteSink&) = delete;
  virtual ~ByteSink() {}

  // Appends the "n" bytes starting at "bytes".
  virtual void Append(const char* bytes, size_t n) = 0;

  // Flushes internal buffers. The default implementation does nothing. ByteSink
  // subclasses may use internal buffers that require calling Flush() at the end
  // of the stream.
  virtual void Flush();
};

// An abstract interface for an object that produces a fixed-size sequence of
// bytes.
//
// Example:
//
//   ByteSource* source = ...
//   while (source->Available() > 0) {
//     absl::string_view data = source->Peek();
//     ... do something with "data" ...
//     source->Skip(data.length());
//   }
//
class ByteSource {
 public:
  ByteSource() {}
  ByteSource(const ByteSource&) = delete;
  ByteSource& operator=(const ByteSource&) = delete;
  virtual ~ByteSource() {}

  // Returns the number of bytes left to read from the source. Available()
  // should decrease by N each time Skip(N) is called. Available() may not
  // increase. Available() returning 0 indicates that the ByteSource is
  // exhausted.
  //
  // Note: Size() may have been a more appropriate name as it's more
  //       indicative of the fixed-size nature of a ByteSource.
  virtual size_t Available() const = 0;

  // Returns an absl::string_view of the next contiguous region of the source.
  // Does not reposition the source. The returned region is empty iff
  // Available() == 0.
  //
  // The returned region is valid until the next call to Skip() or until this
  // object is destroyed, whichever occurs first.
  //
  // The length of the returned absl::string_view will be <= Available().
  virtual absl::string_view Peek() = 0;

  // Skips the next n bytes. Invalidates any absl::string_view returned by a
  // previous call to Peek().
  //
  // REQUIRES: Available() >= n
  virtual void Skip(size_t n) = 0;

  // Writes the next n bytes in this ByteSource to the given ByteSink, and
  // advances this ByteSource past the copied bytes. The default implementation
  // of this method just copies the bytes normally, but subclasses might
  // override CopyTo to optimize certain cases.
  //
  // REQUIRES: Available() >= n
  virtual void CopyTo(ByteSink* sink, size_t n);
};

//
// Some commonly used implementations of ByteSink
//

// Implementation of ByteSink that writes to an unsized byte array. No
// bounds-checking is performed--it is the caller's responsibility to ensure
// that the destination array is large enough.
//
// Example:
//
//   char buf[10];
//   UncheckedArrayByteSink sink(buf);
//   sink.Append("hi", 2);    // OK
//   sink.Append(data, 100);  // WOOPS! Overflows buf[10].
//
class UncheckedArrayByteSink : public ByteSink {
 public:
  UncheckedArrayByteSink(const UncheckedArrayByteSink&) = delete;
  UncheckedArrayByteSink& operator=(const UncheckedArrayByteSink&) = delete;
  explicit UncheckedArrayByteSink(char* dest) : dest_(dest) {}
  virtual void Append(const char* data, size_t n) override;

  // Returns the current output pointer so that a caller can see how many bytes
  // were produced.
  //
  // Note: this method is not part of the ByteSink interface.
  char* CurrentDestination() const { return dest_; }

 private:
  char* dest_;
};

// Implementation of ByteSink that writes to a sized byte array. This sink will
// not write more than "capacity" bytes to outbuf. Once "capacity" bytes are
// appended, subsequent bytes will be ignored and Overflowed() will return true.
// Overflowed() does not cause a runtime error (i.e., it does not CHECK fail).
//
// Example:
//
//   char buf[10];
//   CheckedArrayByteSink sink(buf, 10);
//   sink.Append("hi", 2);    // OK
//   sink.Append(data, 100);  // Will only write 8 more bytes
//
class CheckedArrayByteSink : public ByteSink {
 public:
  CheckedArrayByteSink(char* outbuf, size_t capacity);
  CheckedArrayByteSink(const CheckedArrayByteSink&) = delete;
  CheckedArrayByteSink& operator=(const CheckedArrayByteSink&) = delete;
  virtual void Append(const char* bytes, size_t n) override;

  // Returns the number of bytes actually written to the sink.
  size_t NumberOfBytesWritten() const { return size_; }

  // Returns true if any bytes were discarded, i.e., if there was an
  // attempt to write more than 'capacity' bytes.
  bool Overflowed() const { return overflowed_; }

 private:
  char* outbuf_;
  const size_t capacity_;
  size_t size_;
  bool overflowed_;
};

// Implementation of ByteSink that allocates an internal buffer (a char array)
// and expands it as needed to accommodate appended data (similar to a string),
// and allows the caller to take ownership of the internal buffer via the
// GetBuffer() method. The buffer returned from GetBuffer() must be deleted by
// the caller with delete[]. GetBuffer() also sets the internal buffer to be
// empty, and subsequent appends to the sink will create a new buffer. The
// destructor will free the internal buffer if GetBuffer() was not called.
//
// Example:
//
//   GrowingArrayByteSink sink(10);
//   sink.Append("hi", 2);
//   sink.Append(data, n);
//   const char* buf = sink.GetBuffer();  // Ownership transferred
//   delete[] buf;
//
class GrowingArrayByteSink : public strings::ByteSink {
 public:
  explicit GrowingArrayByteSink(size_t estimated_size);
  GrowingArrayByteSink(const GrowingArrayByteSink&) = delete;
  GrowingArrayByteSink& operator=(const GrowingArrayByteSink&) = delete;
  virtual ~GrowingArrayByteSink();
  virtual void Append(const char* bytes, size_t n) override;

  // Returns the allocated buffer, and sets nbytes to its size. The caller takes
  // ownership of the buffer and must delete it with delete[].
  char* GetBuffer(size_t* nbytes);

 private:
  void Expand(size_t amount);
  void ShrinkToFit();

  size_t capacity_;
  char* buf_;
  size_t size_;
};

// Implementation of ByteSink that appends to the given string.
// Existing contents of "dest" are not modified; new data is appended.
//
// Example:
//
//   string dest = "Hello ";
//   StringByteSink sink(&dest);
//   sink.Append("World", 5);
//   assert(dest == "Hello World");
//
class StringByteSink : public ByteSink {
 public:
  explicit StringByteSink(std::string* dest) : dest_(dest) {}
  StringByteSink(const StringByteSink&) = delete;
  StringByteSink& operator=(const StringByteSink&) = delete;
  virtual void Append(const char* data, size_t n) override;

 private:
  std::string* dest_;
};

// Implementation of ByteSink that discards all data.
//
// Example:
//
//   NullByteSink sink;
//   sink.Append(data, data.size());  // All data ignored.
//
class NullByteSink : public ByteSink {
 public:
  NullByteSink() {}
  NullByteSink(const NullByteSink&) = delete;
  NullByteSink& operator=(const NullByteSink&) = delete;
  void Append(const char* /*data*/, size_t /*n*/) override {}
};

//
// Some commonly used implementations of ByteSource
//

// Implementation of ByteSource that reads from an absl::string_view.
//
// Example:
//
//   string data = "Hello";
//   ArrayByteSource source(data);
//   assert(source.Available() == 5);
//   assert(source.Peek() == "Hello");
//
class ArrayByteSource : public ByteSource {
 public:
  explicit ArrayByteSource(absl::string_view s) : input_(s) {}
  ArrayByteSource(const ArrayByteSource&) = delete;
  ArrayByteSource& operator=(const ArrayByteSource&) = delete;

  virtual size_t Available() const override;
  virtual absl::string_view Peek() override;
  virtual void Skip(size_t n) override;

 private:
  absl::string_view input_;
};

// Implementation of ByteSource that wraps another ByteSource, limiting the
// number of bytes returned.
//
// The caller maintains ownership of the underlying source, and may not use the
// underlying source while using the LimitByteSource object.  The underlying
// source's pointer is advanced by n bytes every time this LimitByteSource
// object is advanced by n.
//
// Example:
//
//   string data = "Hello World";
//   ArrayByteSource abs(data);
//   assert(abs.Available() == data.size());
//
//   LimitByteSource limit(abs, 5);
//   assert(limit.Available() == 5);
//   assert(limit.Peek() == "Hello");
//
class LimitByteSource : public ByteSource {
 public:
  // Returns at most "limit" bytes from "source".
  LimitByteSource(ByteSource* source, size_t limit);

  virtual size_t Available() const override;
  virtual absl::string_view Peek() override;
  virtual void Skip(size_t n) override;

  // We override CopyTo so that we can forward to the underlying source, in
  // case it has an efficient implementation of CopyTo.
  virtual void CopyTo(ByteSink* sink, size_t n) override;

 private:
  ByteSource* source_;
  size_t limit_;
};

}  // namespace strings
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_STUBS_BYTESTREAM_H_
