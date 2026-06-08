#include <datadog/baggage.h>

#include "catch.hpp"
#include "mocks/dict_readers.h"
#include "mocks/dict_writers.h"
#include "random.h"

#define BAGGAGE_TEST(x) TEST_CASE(x, "[baggage]")

using namespace datadog::tracing;

BAGGAGE_TEST("missing baggage header is not an error") {
  MockDictReader reader;
  auto maybe_baggage = Baggage::extract(reader);
  CHECK(!maybe_baggage);
}

BAGGAGE_TEST("extract") {
  SECTION("parsing") {
    struct TestCase final {
      std::string name;
      std::string input;
      Expected<Baggage, Baggage::Error> expected_baggage;
    };

    auto test_case = GENERATE(values<TestCase>({
        {
            "empty baggage header",
            "",
            Baggage(),
        },
        {
            "only spaces",
            "                  ",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
        {
            "valid",
            "key1=value1,key2=value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 1",
            "    key1=value1,key2=value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 2",
            "    key1    =value1,key2=value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 3",
            "    key1    = value1,key2=value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 4",
            "    key1    = value1  ,key2=value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 5",
            "    key1    = value1  , key2=value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 6",
            "    key1    = value1  , key2  =value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 7",
            "    key1    = value1  , key2  =   value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 8",
            "    key1    = value1  , key2  =   value2  ",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "leading spaces 9",
            "key1   = value1,   key2=   value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "spaces in key is not allowed",
            "key1 foo=value1",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
        {
            "spaces in value is not allowed",
            "key1=value1 value2",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
        {
            "ignore properties",
            "key1=value1;a=b,key2=value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "ignore properties 2",
            "key1=value1     ;foo=bar,key2=value2",
            Baggage({{"key1", "value1"}, {"key2", "value2"}}),
        },
        {
            "ignore properties 3",
            "key1=value1, key2 = value2;property1;property2, key3=value3; "
            "propertyKey=propertyValue",
            Baggage({
                {"key1", "value1"},
                {"key2", "value2"},
                {"key3", "value3"},
            }),
        },
        {
            "malformed baggage",
            ",k1=v1,k2=v2,",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
        {
            "malformed baggage 2",
            "=",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
        {
            "malformed baggage 3",
            "=,key2=value2",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
        {
            "malformed baggage 4",
            "key1=value1,=",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
        {
            "malformed baggage 5",
            "key1=value1,key2=",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
        {
            "malformed baggage 6",
            "key1=",
            Baggage::Error{Baggage::Error::MALFORMED_BAGGAGE_HEADER},
        },
    }));

    CAPTURE(test_case.name, test_case.input);

    const std::unordered_map<std::string, std::string> headers{
        {"baggage", test_case.input}};
    MockDictReader reader(headers);

    auto maybe_baggage = Baggage::extract(reader);
    if (maybe_baggage.has_value() && test_case.expected_baggage.has_value()) {
      CHECK(*maybe_baggage == *test_case.expected_baggage);
    } else if (maybe_baggage.if_error() &&
               test_case.expected_baggage.if_error()) {
      CHECK(maybe_baggage.error().code ==
            test_case.expected_baggage.error().code);
    } else {
      FAIL("mistmatch between what is expected and the result");
    }
  }
}

BAGGAGE_TEST("inject") {
  SECTION("custom items limit is respected") {
    Baggage bag({{"violets", "blue"}, {"roses", "red"}});

    const Baggage::Options opts_items_limit{
        /*.max_bytes = */ 2048,
        /*.max_items =*/1,
    };

    MockDictWriter writer;
    auto injected = bag.inject(writer, opts_items_limit);

    REQUIRE(!injected);
    REQUIRE(writer.items.count("baggage") == 1);
    CHECK((writer.items["baggage"] == "violets=blue" ||
           writer.items["baggage"] == "roses=red"));
  }

  SECTION("custom bytes limit is respected") {
    Baggage bag({{"foo", "bar"}, {"a", "b"}, {"hello", "world"}});

    const std::string expected_baggage{"foo=bar,a=b"};
    const Baggage::Options opts_bytes_limit{
        /*.max_bytes = */ expected_baggage.size(),
        /*.max_items =*/1000,
    };

    MockDictWriter writer;
    auto injected = bag.inject(writer, opts_bytes_limit);

    REQUIRE(!injected);
    REQUIRE(writer.items.count("baggage") == 1);
    REQUIRE(writer.items["baggage"].size() <= opts_bytes_limit.max_bytes);
    CHECK((writer.items["baggage"] == expected_baggage ||
           writer.items["baggage"] == "hello=world"));
  }

  SECTION("default limits are respected") {
    auto default_opts = Baggage::default_options;
    SECTION("max items reached") {
      std::size_t max_bytes_needed = 0;

      Baggage bag;
      for (size_t i = 0; i < default_opts.max_items; ++i) {
        auto uuid_value = uuid();
        bag.set(uuid_value, "a");
        max_bytes_needed +=
            uuid_value.size() + 1 + 2;  // +2 are for the separators
      }
      // NOTE(@dmehala): if that fails, the flackiness is comming from UUIDs
      // collision.
      REQUIRE(bag.size() == default_opts.max_items);
      bag.set("a", "a");
      max_bytes_needed += 4;

      Baggage::Options opts = Baggage::default_options;
      opts.max_bytes = max_bytes_needed;

      MockDictWriter writer;
      auto injected = bag.inject(writer, opts);
      CHECK(!injected);
      CHECK(injected.error().code ==
            Error::Code::BAGGAGE_MAXIMUM_ITEMS_REACHED);
    }

    SECTION("max bytes reached") {
      std::string v(default_opts.max_bytes, '-');
      Baggage bag({{"a", v}, {"b", v}});

      MockDictWriter writer;
      auto injected = bag.inject(writer);
      REQUIRE(!injected);
      CHECK(injected.error().code ==
            Error::Code::BAGGAGE_MAXIMUM_BYTES_REACHED);
    }
  }
}

BAGGAGE_TEST("round-trip") {
  Baggage bag({
      {"team", "proxy"},
      {"company", "datadog"},
      {"user", "dmehala"},
  });

  MockDictWriter writer;
  REQUIRE(bag.inject(writer));

  MockDictReader reader(writer.items);
  auto extracted_baggage = Baggage::extract(reader);
  REQUIRE(extracted_baggage);

  CHECK(*extracted_baggage == bag);
}

BAGGAGE_TEST("accessors") {
  Baggage bag({{"foo", "bar"}, {"answer", "42"}, {"dog", "woof"}});

  const std::string baggage = "team=proxy,company=datadog,user=dmehala";
  const std::unordered_map<std::string, std::string> headers{
      {"baggage", baggage}};
  MockDictReader reader(headers);

  auto maybe_baggage = Baggage::extract(reader);
  REQUIRE(maybe_baggage);

  CHECK(maybe_baggage->size() == 3);

  CHECK(maybe_baggage->get("company") == "datadog");
  CHECK(!maybe_baggage->get("boogaloo"));
  CHECK(maybe_baggage->contains("boogaloo") == false);
  CHECK(maybe_baggage->contains("team") == true);

  maybe_baggage->set("color", "red");
  /// NOTE: ensure `set` overwrite
  maybe_baggage->set("color", "blue");
  CHECK(maybe_baggage->get("color") == "blue");
  CHECK(maybe_baggage->size() == 4);

  maybe_baggage->remove("company");
  CHECK(maybe_baggage->contains("company") == false);
  CHECK(maybe_baggage->size() == 3);

  SECTION("visit") {
    bag.visit([](StringView key, StringView value) {
      (void)key;
      (void)value;
    });
  }

  SECTION("clear") {
    bag.clear();
    CHECK(bag.empty() == true);
  }
}
