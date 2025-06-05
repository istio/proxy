# hessian2-codec

![CI](https://github.com/alibaba/hessian2-codec/workflows/CI/badge.svg?branch=main)
[![License](https://img.shields.io/badge/license-Apache%202-4EB1BA.svg)](https://www.apache.org/licenses/LICENSE-2.0.html)
![Coverage](https://codecov.io/gh/alibaba/hessian2-codec/branch/main/graph/badge.svg)


**hessian2-codec** is a C++ library from Alibaba for hessian2 codec. It is a complete C++ implementation of [hessian2 spec](http://hessian.caucho.com/doc/hessian-serialization.html). Because it was originally intended to implement the Dubbo Filter of Envoy, it did not provide good support for serialization of user-defined types (there is only one way to implement user-defined types using ADL, but it is not very complete and does not support nested types well).
At the moment it is simply deserializing content into some C++ intermediate types.

## Getting Started

### Install

1. To download and install Bazel (and any of its dependencies), consult the [Bazel Installation Guide](https://docs.bazel.build/versions/master/install.html).
2. Refer to [Supported Platforms](#supported-platforms) installation related compiler.
3. Use hessian2-codec, see the demo directory for details.

```shell
$ cd demo
$ bazel build //...
$ ./bazel-bin/demo
```
### Basic usage

```C++
#include <iostream>

#include "hessian2/codec.hpp"
#include "hessian2/basic_codec/object_codec.hpp"

int main() {
  {
    std::string out;
    ::Hessian2::Encoder encode(out);
    encode.encode<std::string>("test string");
    ::Hessian2::Decoder decode(out);
    auto ret = decode.decode<std::string>();
    if (ret) {
      std::cout << *ret << std::endl;
    } else {
      std::cerr << "decode failed: " << decode.getErrorMessage() << std::endl;
    }
  }
  {
    std::string out;
    ::Hessian2::Encoder encode(out);
    encode.encode<int64_t>(100);
    ::Hessian2::Decoder decode(out);
    auto ret = decode.decode<int64_t>();
    if (ret) {
      std::cout << *ret << std::endl;
    } else {
      std::cerr << "decode failed: " << decode.getErrorMessage() << std::endl;
    }
  }

  return 0;
}
```


### Advance usage

1. Implement the serialization and deserialization of custom types with ADL

```C++
#include <iostream>

#include "hessian2/codec.hpp"
#include "hessian2/basic_codec/object_codec.hpp"

struct Person {
  int32_t age_{0};
  std::string name_;
};

// The custom struct needs to implement from_hessian and to_hessian methods to
// encode and decode

void fromHessian(Person&, ::Hessian2::Decoder&);
bool toHessian(const Person&, ::Hessian2::Encoder&);

void fromHessian(Person& p, ::Hessian2::Decoder& d) {
  auto age = d.decode<int32_t>();
  if (age) {
    p.age_ = *age;
  }

  auto name = d.decode<std::string>();
  if (name) {
    p.name_ = *name;
  }
}

bool toHessian(const Person& p, ::Hessian2::Encoder& e) {
  e.encode<int32_t>(p.age_);
  e.encode<std::string>(p.name_);
  return true;
}

int main() {
  std::string out;
  Hessian2::Encoder encode(out);
  Person s;
  s.age_ = 12;
  s.name_ = "test";

  encode.encode<Person>(s);
  Hessian2::Decoder decode(out);
  auto decode_person = decode.decode<Person>();
  if (!decode_person) {
    std::cerr << "hessian decode failed " << decode.getErrorMessage()
              << std::endl;
    return -1;
  }
  std::cout << "Age: " << decode_person->age_
            << " Name: " << decode_person->name_ << std::endl;
}
```

There is currently no way to serialize container nested custom types such as`std::list<Person>`.


2. Customize Reader and Writer

Hessian2-codec uses the `std::string` implementation of reader and Writer by default, although we can customize both implementations.

```C++

#include <vector>
#include <iostream>

#include "hessian2/codec.hpp"
#include "hessian2/basic_codec/object_codec.hpp"
#include "hessian2/reader.hpp"
#include "hessian2/writer.hpp"

#include "absl/strings/string_view.h"

struct Slice {
  const uint8_t* data_;
  size_t size_;
};

class SliceReader : public ::Hessian2::Reader {
 public:
  SliceReader(Slice buffer) : buffer_(buffer){};
  virtual ~SliceReader() = default;

  virtual void rawReadNBytes(void* out, size_t len,
                             size_t peek_offset) override {
    ABSL_ASSERT(byteAvailable() + peek_offset >= len);
    uint8_t* dest = static_cast<uint8_t*>(out);
    // offset() Returns the current position that has been read.
    memcpy(dest, buffer_.data_ + offset() + peek_offset, len);
  }
  virtual uint64_t length() const override { return buffer_.size_; }

 private:
  Slice buffer_;
};

class VectorWriter : public ::Hessian2::Writer {
 public:
  VectorWriter(std::vector<uint8_t>& data) : data_(data) {}
  ~VectorWriter() = default;
  virtual void rawWrite(const void* data, uint64_t size) {
    const char* src = static_cast<const char*>(data);
    for (size_t i = 0; i < size; i++) {
      data_.push_back(src[i]);
    }
  }
  virtual void rawWrite(absl::string_view data) {
    for (auto& ch : data) {
      data_.push_back(ch);
    }
  }

 private:
  std::vector<uint8_t>& data_;
};

int main() {
  std::vector<uint8_t> data;
  auto writer = std::make_unique<VectorWriter>(data);

  ::Hessian2::Encoder encode(std::move(writer));
  encode.encode<std::string>("test string");
  Slice s{static_cast<const uint8_t*>(data.data()), data.size()};
  auto reader = std::make_unique<SliceReader>(s);
  ::Hessian2::Decoder decode(std::move(reader));
  auto ret = decode.decode<std::string>();
  if (ret) {
    std::cout << *ret << std::endl;
  } else {
    std::cerr << "decode failed: " << decode.getErrorMessage() << std::endl;
  }
}
```

## Type mapping

C++ does not have a global parent like Java Object, so there is no single type that can represent all hessian types, so we create an Object base class from which all hessian types are inherited.

| hessian type |  java type  |  C++ type |
| --- | --- | --- |
| **null** | null | NullObject |
| **binary** | byte[] | BinaryObject |
| **boolean** | boolean | BooleanObject |
| **date** | java.util.Date | DateObject |
| **double** | double | DoubleObject |
| **int** | int | IntegerObject |
| **long** | long | LongObject |
| **string** | java.lang.String | StringObject |
| **untyped list** | java.util.List | UntypedListObject |
| **typed list** | java.util.ArrayList | TypedListObject |
| **untyped map** | java.util.Map | UntypedMapObject |
| **typed map** | for some OO language | TypedMapObject |
| **object** | custom define object | ClassInstance|



## Supported Platforms
hessian2-codec requires a codebase and compiler compliant with the C++14 standard or
newer.

The hessian2-codec code is officially supported on the following platforms.
Operating systems or tools not listed below are community-supported. For
community-supported platforms, patches that do not complicate the code may be
considered.

If you notice any problems on your platform, please file an issue on the
[hessian2-codec GitHub Issue Tracker](https://github.com/alibaba/hessian2-codec/issues).
Pull requests containing fixes are welcome!

### Operating Systems

* Linux
* macOS
* Windows(Theoretically, yes, but I haven't tested it.)

### Compilers

* gcc 7.0+
* clang 7.0+

### Build Systems

* [Bazel](https://bazel.build/)

## Who Is Using hessian2-codec?

In addition to many internal projects at Alibaba, hessian2-codec is also used by the
following notable projects:

*   The [Envoy](https://www.envoyproxy.io/) (Dubbo Filter in Envoy will use hessian2-codec as the serializer).

## Related Open Source Projects

* [dubbo-go-hessian2](https://github.com/apache/dubbo-go-hessian2) is a GO language implementation of the Hessian2 serializer.
* [dubbo-hessian-lite](https://github.com/apache/dubbo-hessian-lite) is a Java language implementation of the Hessian2 serializer.


## Contributing

Please read [`CONTRIBUTING.md`](CONTRIBUTING.md) for details on how to
contribute to this project.

Happy testing!

## Develop

Generate `compile_commands.json` for this repo by `bazel run :refresh_compile_commands`. Thank https://github.com/hedronvision/bazel-compile-commands-extractor for it provide the great script/tool to make this so easy!

## License
hessian2-codec is distributed under Apache License 2.0.

## Acknowledgements
* [test_hessian](https://github.com/apache/dubbo-go-hessian2/tree/master/test_hessian) A comprehensive Hessian2 testing framework.
