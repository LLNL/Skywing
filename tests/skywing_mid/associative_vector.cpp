#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"
#include "skywing_mid/associative_vector.hpp"

using namespace skywing;

TEST_CASE("Associative Vector", "[Skywing_AssociativeVector]")
{
  using AV1 = AssociativeVector<std::uint32_t, std::int32_t, true>;
  AV1 a;
  REQUIRE(a.get_default_value() == 0);
  a[1] = 1;
  a[2] = 1;
  a[4] = 1;
  AV1 b;
  REQUIRE(a.size() == 3);
  a += b;
  REQUIRE(a.size() == 3);
  REQUIRE(a[1] == 1);
  REQUIRE(a[2] == 1);
  REQUIRE(a[4] == 1);
  b[1] = 1;
  b[2] = 2;
  b[3] = 3;
  a += b;
  REQUIRE(a.size() == 4);
  REQUIRE(a[1] == 2);
  REQUIRE(a[2] == 3);
  REQUIRE(a[3] == 3);
  REQUIRE(a[4] == 1);
  REQUIRE(a.get_keys().size() == 4);

  REQUIRE(a.dot(b) == 17);
  REQUIRE(a.dot(a) == 23);

  a -= b;
  REQUIRE(a[1] == 1);
  REQUIRE(a[2] == 1);
  REQUIRE(a[3] == 0);
  REQUIRE(a[4] == 1);

  AV1 c = a + b;
  REQUIRE(c[1] == 2);
  REQUIRE(c[2] == 3);
  REQUIRE(c[3] == 3);
  REQUIRE(c[4] == 1);

  c *= 3;
  REQUIRE(c[1] == 6);
  REQUIRE(c[2] == 9);
  REQUIRE(c[3] == 9);
  REQUIRE(c[4] == 3);

  a = 3 * c;
  REQUIRE(a[1] == 18);
  REQUIRE(a[2] == 27);
  REQUIRE(a[3] == 27);
  REQUIRE(a[4] == 9);

  a = -a;
  REQUIRE(a[1] == -18);
  REQUIRE(a[2] == -27);
  REQUIRE(a[3] == -27);
  REQUIRE(a[4] == -9);



  using AV2 = AssociativeVector<std::uint32_t, std::int32_t, false>; // closed AssociativeVector
  AV2 aa(std::vector<std::uint32_t>{1, 2, 4}, 0);
  REQUIRE(aa.get_default_value() == 0);
  REQUIRE(aa.size() == 3);
  aa[1] = 1;
  aa[2] = 1;
  aa[4] = 1;
  AV2 bb(std::vector<std::uint32_t>{1, 2, 3}, 0);
  REQUIRE(bb.size() == 3);
  aa += bb;
  REQUIRE(aa.size() == 3);
  REQUIRE(aa[1] == 1);
  REQUIRE(aa[2] == 1);
  REQUIRE(aa[4] == 1);
  bb[1] = 1;
  bb[2] = 2;
  bb[3] = 3;
  aa += bb;
  REQUIRE(aa.size() == 3);
  REQUIRE(aa[1] == 2);
  REQUIRE(aa[2] == 3);
  REQUIRE(aa[4] == 1);
  REQUIRE(aa.get_keys().size() == 3);

  REQUIRE(aa.dot(bb) == 8);
  REQUIRE(aa.dot(aa) == 14);

  aa -= bb;
  REQUIRE(aa.size() == 3);
  REQUIRE(aa[1] == 1);
  REQUIRE(aa[2] == 1);
  REQUIRE(aa[4] == 1);

  // testing pubsub conversion
  aa += bb;
  using pubsub_type = std::tuple<std::int32_t, std::vector<std::uint32_t>, std::vector<std::int32_t>>;
  pubsub_type ps = PubSubConverter<AV2>::convert(aa);
  REQUIRE(std::get<0>(ps) == 0);
  REQUIRE(std::get<1>(ps).size() == 3);
  REQUIRE(std::get<2>(ps).size() == 3);
  AV2 cc = PubSubConverter<AV2>::deconvert(ps);
  REQUIRE(cc.size() == 3);
  REQUIRE(cc[1] == 2);
  REQUIRE(cc[2] == 3);
  REQUIRE(cc[4] == 1);
  REQUIRE(cc.get_keys().size() == 3);
  
}

