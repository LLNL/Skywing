#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"
#include "skywing_mid/pubsub_converter.hpp"

using namespace skywing;

  struct Colin { std::int32_t age; };
    
  template<>
  struct skywing::PubSubConverter<Colin>
  {
    using pubsub_type = std::int32_t;
    static pubsub_type convert(Colin c) { return c.age; }
    static Colin deconvert(pubsub_type in) { return Colin{in}; }
  };

  struct SirWalter { std::int64_t meows; double purrs; };

  template<>
  struct skywing::PubSubConverter<SirWalter>
  {
    using pubsub_type = std::tuple<std::int64_t, double>;
    static pubsub_type convert(SirWalter sw) { return std::make_tuple(sw.meows, sw.purrs); }
    static SirWalter deconvert(pubsub_type in)
    { return SirWalter{std::get<0>(in), std::get<1>(in)}; }
  };


TEST_CASE("PubSub Converter", "[Skywing_PubSubConverter]")
{
  using myVec_t = std::vector<Colin>;
  using nestedVec_t = std::vector<myVec_t>;
  myVec_t mv1{Colin{1}, Colin{2}};
  myVec_t mv2{Colin{3}, Colin{4}};
  nestedVec_t nestedv{mv1, mv2};
  using psNestedV_t = std::tuple<std::vector<std::uint32_t>, std::vector<std::int32_t>>;
  psNestedV_t ps = PubSubConverter<nestedVec_t>::convert(nestedv);

  REQUIRE(std::get<0>(ps).size() == 2);
  REQUIRE(std::get<1>(ps).size() == 4);
  REQUIRE(std::get<0>(ps)[0] == 2);
  REQUIRE(std::get<0>(ps)[1] == 2);
  REQUIRE(std::get<1>(ps)[0] == 1);
  REQUIRE(std::get<1>(ps)[1] == 2);
  REQUIRE(std::get<1>(ps)[2] == 3);
  REQUIRE(std::get<1>(ps)[3] == 4);
  nestedVec_t rv = PubSubConverter<nestedVec_t>::deconvert(ps);

  REQUIRE(rv.size() == 2);
  REQUIRE(rv[0].size() == 2);
  REQUIRE(rv[1].size() == 2);
  REQUIRE(rv[0][0].age == 1);
  REQUIRE(rv[0][1].age == 2);
  REQUIRE(rv[1][0].age == 3);
  REQUIRE(rv[1][1].age == 4);

  using mySWVec_t = std::vector<SirWalter>;
  using nestedSWVec_t = std::vector<mySWVec_t>;
  mySWVec_t swv1{SirWalter{1, 1.5}, SirWalter{2, 2.5}};
  mySWVec_t swv2{SirWalter{3, 3.5}, SirWalter{4, 4.5}};
  nestedSWVec_t nestedswv{swv1, swv2};
  using psNestedSWV_t = std::tuple<std::vector<std::uint32_t>, std::vector<std::int64_t>, std::vector<double>>;
  psNestedSWV_t swps = PubSubConverter<nestedSWVec_t>::convert(nestedswv);

  REQUIRE(std::get<0>(swps).size() == 2);
  REQUIRE(std::get<1>(swps).size() == 4);
  REQUIRE(std::get<2>(swps).size() == 4);
  REQUIRE(std::get<0>(swps)[0] == 2);
  REQUIRE(std::get<0>(swps)[1] == 2);
  REQUIRE(std::get<1>(swps)[0] == 1);
  REQUIRE(std::get<1>(swps)[1] == 2);
  REQUIRE(std::get<1>(swps)[2] == 3);
  REQUIRE(std::get<1>(swps)[3] == 4);
  REQUIRE(std::get<2>(swps)[0] == 1.5);
  REQUIRE(std::get<2>(swps)[1] == 2.5);
  REQUIRE(std::get<2>(swps)[2] == 3.5);
  REQUIRE(std::get<2>(swps)[3] == 4.5);
  nestedSWVec_t swrv = PubSubConverter<nestedSWVec_t>::deconvert(swps);

  REQUIRE(swrv.size() == 2);
  REQUIRE(swrv[0].size() == 2);
  REQUIRE(swrv[1].size() == 2);
  REQUIRE(swrv[0][0].meows == 1);
  REQUIRE(swrv[0][1].meows == 2);
  REQUIRE(swrv[1][0].meows == 3);
  REQUIRE(swrv[1][1].meows == 4);
  REQUIRE(swrv[0][0].purrs == 1.5);
  REQUIRE(swrv[0][1].purrs == 2.5);
  REQUIRE(swrv[1][0].purrs == 3.5);
  REQUIRE(swrv[1][1].purrs == 4.5);
  
}

