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
