// Tests various assorted things that don't really belong in their own tests

#include <catch2/catch.hpp>

#include "skywing_core/internal/utility/algorithms.hpp"

#include <array>
#include <vector>

using namespace skywing::internal;

TEST_CASE("Split", "[Skywing_Split]")
{
  using namespace std::literals;
  using vec = std::vector<std::string_view>;
  std::string test_string{"test string"};
  REQUIRE(split("test string"s, ' ') == vec{"test"sv, "string"sv});
  REQUIRE(split("weird\1char"s, '\1') == vec{"weird"sv, "char"sv});
  REQUIRE(split("null\0embed"s, '\0') == vec{"null"sv, "embed"sv});
  REQUIRE(split("nothing to split"s, '\n') == vec{"nothing to split"sv});
  REQUIRE(split("test limit of 2"s, ' ', 2) == vec{"test"sv, "limit of 2"sv});
}

TEST_CASE("Concatenate", "[Skywing_Concatenate]")
{
  std::vector<int> vals1{1, 2, 3};
  std::array<int, 3> vals2{4, 5, 6};
  REQUIRE(concatenate(vals1, vals2) == std::vector<int>{1, 2, 3, 4, 5, 6});
}
