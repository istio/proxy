// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_COMMON_STRUCTURED_HEADERS_H_
#define QUICHE_COMMON_STRUCTURED_HEADERS_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_export.h"
#include "quiche/common/platform/api/quiche_logging.h"

namespace quiche {
namespace structured_headers {

// This file implements parsing of HTTP structured headers, as defined in
// RFC8941 (https://www.rfc-editor.org/rfc/rfc8941.html). For compatibility with
// the shipped implementation of Web Packaging, this file also supports a
// previous revision of the standard, referred to here as "Draft 9".
// (https://datatracker.ietf.org/doc/draft-ietf-httpbis-header-structure/09/)
//
// The major difference between the two revisions is in the various list
// formats: Draft 9 describes "parameterised lists" and "lists-of-lists", while
// the final RFC uses a single "list" syntax, whose members may be inner lists.
// There should be no ambiguity, however, as the code which calls this parser
// should be expecting only a single type for a given header.
//
// References within the code are tagged with either [SH09] or [RFC8941],
// depending on which revision they refer to.
//
// Currently supported data types are:
//  Item:
//   integer: 123
//   string: "abc"
//   token: abc
//   byte sequence: *YWJj*
//  Parameterised list: abc_123;a=1;b=2; cdef_456, ghi;q="9";r="w"
//  List-of-lists: "foo";"bar", "baz", "bat"; "one"
//  List: "foo", "bar", "It was the best of times."
//        ("foo" "bar"), ("baz"), ("bat" "one"), ()
//        abc;a=1;b=2; cde_456, (ghi jkl);q="9";r=w
//  Dictionary: a=(1 2), b=3, c=4;aa=bb, d=(5 6);valid=?0
//
// Functions are provided to parse each of these, which are intended to be
// called with the complete value of an HTTP header (that is, any
// sub-structure will be handled internally by the parser; the exported
// functions are not intended to be called on partial header strings.) Input
// values should be ASCII byte strings (non-ASCII characters should not be
// present in Structured Header values, and will cause the entire header to fail
// to parse.)

class QUICHE_EXPORT Item {
 public:
  enum ItemType {
    kNullType,
    kIntegerType,
    kDecimalType,
    kStringType,
    kTokenType,
    kByteSequenceType,
    kBooleanType
  };

  struct string_t {
    constexpr explicit string_t() = default;
  };
  static constexpr string_t string{};

  struct token_t {
    constexpr explicit token_t() = default;
  };
  static constexpr token_t token{};

  struct byte_sequence_t {
    constexpr explicit byte_sequence_t() = default;
  };
  static constexpr byte_sequence_t byte_sequence{};

  struct Token {
    std::string value;

    friend bool operator==(const Token&, const Token&) = default;
  };

  struct ByteSequence {
    std::string value;

    friend bool operator==(const ByteSequence&, const ByteSequence&) = default;
  };

  Item() noexcept;
  explicit Item(int64_t value) noexcept;
  explicit Item(double value) noexcept;
  explicit Item(bool value) noexcept;

  // Prevent pointers from implicitly converting to bool.
  template <typename T>
  explicit Item(const T*) = delete;
  explicit Item(std::nullptr_t) = delete;

  Item(string_t, const char* value);
  Item(string_t, absl::string_view value);
  Item(string_t, std::string value) noexcept;

  Item(token_t, const char* value);
  Item(token_t, absl::string_view value);
  Item(token_t, std::string value) noexcept;

  Item(byte_sequence_t, const char* value);
  Item(byte_sequence_t, absl::string_view value);
  Item(byte_sequence_t, std::string value) noexcept;

  Item(const Item&);
  Item& operator=(const Item&);

  Item(Item&&) noexcept;
  Item& operator=(Item&&) noexcept;

  ~Item();

  QUICHE_EXPORT friend bool operator==(const Item&, const Item&);

  bool is_null() const { return Type() == kNullType; }
  bool is_integer() const { return Type() == kIntegerType; }
  bool is_decimal() const { return Type() == kDecimalType; }
  bool is_string() const { return Type() == kStringType; }
  bool is_token() const { return Type() == kTokenType; }
  bool is_byte_sequence() const { return Type() == kByteSequenceType; }
  bool is_boolean() const { return Type() == kBooleanType; }

  int64_t GetInteger() const {
    const auto* value = GetIfInteger();
    QUICHE_CHECK(value);
    return *value;
  }
  double GetDecimal() const {
    const auto* value = GetIfDecimal();
    QUICHE_CHECK(value);
    return *value;
  }
  bool GetBoolean() const {
    const auto* value = GetIfBoolean();
    QUICHE_CHECK(value);
    return *value;
  }
  const std::string& GetString() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    const auto* value = GetIfString();
    QUICHE_CHECK(value);
    return *value;
  }
  const std::string& GetToken() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    const auto* value = GetIfToken();
    QUICHE_CHECK(value);
    return *value;
  }
  const std::string& GetByteSequence() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    const auto* value = GetIfByteSequence();
    QUICHE_CHECK(value);
    return *value;
  }

  const int64_t* GetIfInteger() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  int64_t* GetIfInteger() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  const double* GetIfDecimal() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  double* GetIfDecimal() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  const std::string* GetIfToken() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  std::string* GetIfToken() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  const std::string* GetIfString() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  std::string* GetIfString() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  const std::string* GetIfByteSequence() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  std::string* GetIfByteSequence() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  const bool* GetIfBoolean() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  bool* GetIfBoolean() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ItemType Type() const { return static_cast<ItemType>(value_.index()); }

  template <typename Visitor>
  auto Visit(Visitor&& visitor) const {
    return std::visit(std::forward<Visitor>(visitor), value_);
  }

  template <typename Visitor>
  auto Visit(Visitor&& visitor) {
    return std::visit(std::forward<Visitor>(visitor), value_);
  }

 private:
  std::variant<std::monostate, int64_t, double, std::string, Token,
               ByteSequence, bool>
      value_;
};

// Returns a human-readable representation of an ItemType.
QUICHE_EXPORT absl::string_view ItemTypeToString(Item::ItemType type);

// Returns `true` if the string is a valid Token value.
QUICHE_EXPORT bool IsValidToken(absl::string_view str);

// Holds a ParameterizedIdentifier (draft 9 only). The contained Item must be a
// Token, and there may be any number of parameters. Parameter ordering is not
// significant.
struct QUICHE_EXPORT ParameterisedIdentifier {
  using Parameters = std::map<std::string, Item, std::less<>>;

  Item identifier;
  Parameters params;

  ParameterisedIdentifier() noexcept;
  ParameterisedIdentifier(const ParameterisedIdentifier&);
  ParameterisedIdentifier& operator=(const ParameterisedIdentifier&);
  ParameterisedIdentifier(ParameterisedIdentifier&&) noexcept;
  ParameterisedIdentifier& operator=(ParameterisedIdentifier&&) noexcept;
  ParameterisedIdentifier(Item, Parameters) noexcept;
  ~ParameterisedIdentifier();

  friend bool operator==(const ParameterisedIdentifier&,
                         const ParameterisedIdentifier&) = default;
};

using Parameters = std::vector<std::pair<std::string, Item>>;

struct QUICHE_EXPORT ParameterizedItem {
  Item item;
  Parameters params;

  ParameterizedItem() noexcept;

  // Convenience constructor for empty parameters.
  explicit ParameterizedItem(Item) noexcept;

  ParameterizedItem(Item, Parameters) noexcept;

  ParameterizedItem(const ParameterizedItem&);
  ParameterizedItem& operator=(const ParameterizedItem&);

  ParameterizedItem(ParameterizedItem&&) noexcept;
  ParameterizedItem& operator=(ParameterizedItem&&) noexcept;

  ~ParameterizedItem();

  friend bool operator==(const ParameterizedItem&,
                         const ParameterizedItem&) = default;
};

// https://www.rfc-editor.org/rfc/rfc8941.html#name-inner-lists
struct QUICHE_EXPORT InnerList {
  std::vector<ParameterizedItem> items;
  Parameters params;

  InnerList() noexcept;

  explicit InnerList(std::vector<ParameterizedItem> items) noexcept;

  InnerList(std::vector<ParameterizedItem> items, Parameters params) noexcept;

  InnerList(const InnerList&);
  InnerList& operator=(const InnerList&);

  InnerList(InnerList&&) noexcept;
  InnerList& operator=(InnerList&&) noexcept;

  ~InnerList();

  friend bool operator==(const InnerList&, const InnerList&) = default;
};

// Holds either a `ParameterizedItem` or an `InnerList`.
//
// TODO(apaseltiner): Use `class` instead of `struct`, since some members are
// private.
struct QUICHE_EXPORT ParameterizedMember {
  explicit ParameterizedMember(ParameterizedItem) noexcept;
  explicit ParameterizedMember(InnerList) noexcept;

  // Constructor for a member that is an inner list.
  ParameterizedMember(std::vector<ParameterizedItem>, Parameters) noexcept;

  // Convenience constructor for a member that is an inner list with empty
  // parameters.
  explicit ParameterizedMember(std::vector<ParameterizedItem>) noexcept;

  // Constructor for a member that is a single Item.
  ParameterizedMember(Item, Parameters) noexcept;

  // Convenience constructor for a member that is a single Item with empty
  // parameters.
  explicit ParameterizedMember(Item) noexcept;

  ParameterizedMember(const ParameterizedMember&);
  ParameterizedMember& operator=(const ParameterizedMember&);

  ParameterizedMember(ParameterizedMember&&) noexcept;
  ParameterizedMember& operator=(ParameterizedMember&&) noexcept;

  ~ParameterizedMember();

  const ParameterizedItem* GetIfItem() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParameterizedItem* GetIfItem() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  const InnerList* GetIfInnerList() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  InnerList* GetIfInnerList() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  template <typename Visitor>
  auto Visit(Visitor&& visitor) const {
    return std::visit(std::forward<Visitor>(visitor), value_);
  }

  template <typename Visitor>
  auto Visit(Visitor&& visitor) {
    return std::visit(std::forward<Visitor>(visitor), value_);
  }

  QUICHE_EXPORT friend bool operator==(const ParameterizedMember&,
                                       const ParameterizedMember&);

  // Deprecated: Explicitly initialize the value to either an inner list or
  // an item using one of the above constructors, or wrap the value in
  // `std::optional`. This constructor shouldn't really exist, as it's not clear
  // what the default should actually be, but it is convenient for code that
  // defers assignment. As is, it produces an invalid value where both
  // `GetIfItem()` and `GetIfInnerList()` return `nullptr`.
  ParameterizedMember() noexcept;

 private:
  std::variant<std::monostate, ParameterizedItem, InnerList> value_;
};

using DictionaryMember = std::pair<std::string, ParameterizedMember>;

// Structured Headers RFC8941 Dictionary.
class QUICHE_EXPORT Dictionary {
 public:
  using iterator = std::vector<DictionaryMember>::iterator;
  using const_iterator = std::vector<DictionaryMember>::const_iterator;
  using key_type = std::string;
  using mapped_type = ParameterizedMember;
  using value_type = DictionaryMember;

  // Tag type to assert that the vector passed to the constructor already
  // contains unique keys.
  struct unique_keys_t {
    constexpr explicit unique_keys_t() = default;
  };
  static constexpr unique_keys_t unique_keys{};

  Dictionary() noexcept;
  Dictionary(const Dictionary&);
  Dictionary(Dictionary&&) noexcept;

  // |members| must not contain duplicate keys (enforced via QUICHE_DCHECK). Use
  // operator[](absl::string_view) when duplicate keys need to overwrite earlier
  // values or when you cannot statically guarantee that the keys are unique.
  Dictionary(unique_keys_t, std::vector<DictionaryMember> members) noexcept;

  // Deprecated (b/580863224): Prefer Dictionary(Dictionary::unique_keys, ...)
  // when keys are guaranteed unique, or operator[](absl::string_view) when
  // duplicate keys need to overwrite earlier values or when you cannot
  // statically guarantee that the keys are unique. This constructor stores
  // |members| without deduplicating, causing key-based accessors (find(), at(),
  // operator[]) to return the first entry for a duplicate key while
  // SerializeDictionary() emits all entries (which resolves to the last entry
  // when re-parsed per RFC 8941 Section 4.2.2).
  explicit Dictionary(std::vector<DictionaryMember> members) noexcept;
  ~Dictionary();
  Dictionary& operator=(const Dictionary&);
  Dictionary& operator=(Dictionary&&) noexcept;

  iterator begin() ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const_iterator begin() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
  iterator end() ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const_iterator end() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // operator[](size_t) and at(size_t) will both abort the program in case of
  // out of bounds access.
  ParameterizedMember& operator[](std::size_t idx)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const ParameterizedMember& operator[](std::size_t idx) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParameterizedMember& at(std::size_t idx) ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const ParameterizedMember& at(std::size_t idx) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Consistent with std::map, if |key| does not exist in the Dictionary, then
  // operator[](absl::string_view) will create an entry for it, but at() will
  // abort the entire program.
  ParameterizedMember& operator[](absl::string_view key)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParameterizedMember& at(absl::string_view key) ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const ParameterizedMember& at(absl::string_view key) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  const_iterator find(absl::string_view key) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  iterator find(absl::string_view key) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void clear();

  bool empty() const;
  std::size_t size() const;
  bool contains(absl::string_view key) const;

  friend bool operator==(const Dictionary&, const Dictionary&) = default;

 private:
  // Uses a vector to hold pairs of key and dictionary member. This makes
  // look up by index and serialization much easier.
  std::vector<DictionaryMember> members_;
};

// Structured Headers Draft 09 Parameterised List.
using ParameterisedList = std::vector<ParameterisedIdentifier>;
// Structured Headers Draft 09 List of Lists.
using ListOfLists = std::vector<std::vector<Item>>;
// Structured Headers RFC8941 List.
using List = std::vector<ParameterizedMember>;

// Returns the result of parsing the header value as an Item, if it can be
// parsed as one, or nullopt if it cannot. Note that this uses the RFC 8941
// parsing rules, and so applies tighter range limits to integers.
//
// When `strict` is true, trailing decimal points are prohibited and byte
// sequences must strictly conform to the specification.
QUICHE_EXPORT std::optional<ParameterizedItem> ParseItem(absl::string_view str,
                                                         bool strict = false);

// Returns the result of parsing the header value as an Item with no parameters,
// or nullopt if it cannot. Note that this uses the RFC 8941 parsing rules, and
// so applies tighter range limits to integers.
//
// When `strict` is true, trailing decimal points are prohibited and byte
// sequences must strictly conform to the specification.
QUICHE_EXPORT std::optional<Item> ParseBareItem(absl::string_view str,
                                                bool strict = false);

// Returns the result of parsing the header value as a Parameterised List, if it
// can be parsed as one, or nullopt if it cannot. Note that parameter keys will
// be returned as strings, which are guaranteed to be ASCII-encoded. List items,
// as well as parameter values, will be returned as Items. This method uses the
// Draft 09 parsing rules for Items, so integers have the 64-bit int range.
// Structured-Headers Draft 09 only.
QUICHE_EXPORT std::optional<ParameterisedList> ParseParameterisedList(
    absl::string_view str);

// Returns the result of parsing the header value as a List of Lists, if it can
// be parsed as one, or nullopt if it cannot. Inner list items will be returned
// as Items. This method uses the Draft 09 parsing rules for Items, so integers
// have the 64-bit int range.
// Structured-Headers Draft 09 only.
QUICHE_EXPORT std::optional<ListOfLists> ParseListOfLists(
    absl::string_view str);

// Returns the result of parsing the header value as a general List, if it can
// be parsed as one, or nullopt if it cannot. RFC 8941 only.
//
// When `strict` is true, trailing decimal points are prohibited and byte
// sequences must strictly conform to the specification.
QUICHE_EXPORT std::optional<List> ParseList(absl::string_view str,
                                            bool strict = false);

// Returns the result of parsing the header value as a general Dictionary, if it
// can be parsed as one, or nullopt if it cannot. RFC 8941 only.
//
// When `strict` is true, trailing decimal points are prohibited and byte
// sequences must strictly conform to the specification.
QUICHE_EXPORT std::optional<Dictionary> ParseDictionary(absl::string_view str,
                                                        bool strict = false);

class QUICHE_EXPORT ItemView final {
 public:
  using string_t = Item::string_t;
  using token_t = Item::token_t;
  using byte_sequence_t = Item::byte_sequence_t;

  static constexpr string_t string{};
  static constexpr token_t token{};
  static constexpr byte_sequence_t byte_sequence{};

  ItemView() noexcept;
  ItemView(int64_t value) noexcept;
  ItemView(double value) noexcept;
  ItemView(bool value) noexcept;

  // Prevent pointers from implicitly converting to bool.
  template <typename T>
  explicit ItemView(const T*) = delete;
  explicit ItemView(std::nullptr_t) = delete;

  ItemView(string_t,
           absl::string_view value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  ItemView(token_t,
           absl::string_view value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  ItemView(byte_sequence_t,
           absl::string_view value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  ItemView(const Item& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  ItemView(const ItemView&) noexcept = default;
  ItemView& operator=(const ItemView&) noexcept = default;

  ItemView(ItemView&&) noexcept = default;
  ItemView& operator=(ItemView&&) noexcept = default;

  ~ItemView() = default;

 private:
  friend class StructuredHeaderSerializer;

  // Wrapper types to permit simplified use of `std::visit`.
  struct Token {
    absl::string_view value;

    explicit Token(
        absl::string_view value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
        : value(value) {}

    Token(const Item::Token& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
        : value(value.value) {}
  };

  struct ByteSequence {
    absl::string_view value;

    explicit ByteSequence(
        absl::string_view value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
        : value(value) {}

    ByteSequence(
        const Item::ByteSequence& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
        : value(value.value) {}
  };

  using Variant = std::variant<std::monostate, int64_t, double,
                               absl::string_view, Token, ByteSequence, bool>;

  Variant value_;
};

// Serialization is implemented for RFC 8941 only.
QUICHE_EXPORT std::optional<std::string> SerializeItem(ItemView value);
QUICHE_EXPORT std::optional<std::string> SerializeItem(
    const ParameterizedItem& value);
QUICHE_EXPORT std::optional<std::string> SerializeList(const List& value);
QUICHE_EXPORT std::optional<std::string> SerializeDictionary(
    const Dictionary& value);

}  // namespace structured_headers
}  // namespace quiche

#endif  // QUICHE_COMMON_STRUCTURED_HEADERS_H_
