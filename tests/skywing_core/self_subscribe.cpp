#include <catch2/catch.hpp>

#include "skywing_core/skywing.hpp"

#include "utils.hpp"

using namespace skywing;

using PubTag = PublishTag<std::int32_t>;
using PrivatePubTag = PrivateTag<std::int32_t>;
using GroupTag = ReduceGroupTag<std::int32_t>;
using ValueTag = ReduceValueTag<std::int32_t>;

const std::array<ValueTag, 2> tags{ValueTag{"Tag 0"}, ValueTag{"Tag 1"}};

std::int32_t reduce_op(std::int32_t a, std::int32_t b) { return a + b; }

constexpr std::chrono::milliseconds wait_time{1000};

TEST_CASE("Self-subscription works", "[Skywing_SelfSubscribe]")
{
  Manager base_manager{get_starting_port(), "Lonely"};

  base_manager.submit_job("job", [&](Job& job, ManagerHandle) {
    // Publish/Subscribe
    const PubTag pub_tag{"integer"};
    job.declare_publication_intent(pub_tag);
    REQUIRE(job.subscribe(pub_tag).wait_for(wait_time));
    job.publish(pub_tag, 10);
    auto pub_fut = job.get_waiter(pub_tag);
    REQUIRE(pub_fut.wait_for(wait_time));
    REQUIRE(pub_fut.get() == 10);

    // IP Publish/Subscribe
    const PrivatePubTag private_pub_tag{"integer"};
    job.declare_publication_intent(private_pub_tag);
    REQUIRE(job.ip_subscribe("localhost:" + std::to_string(get_starting_port()), private_pub_tag).wait_for(wait_time));
    job.publish(private_pub_tag, 30);
    auto private_pub_fut = job.get_waiter(private_pub_tag);
    REQUIRE(private_pub_fut.wait_for(wait_time));
    REQUIRE(private_pub_fut.get() == 30);

    // Reduce operation
    // This is not yet supported - need to discuss if this is wanted as this is a
    // non-trivial addition
    // const GroupTag reduce_tag{"self_reduce"};
    // auto group_fut1 = job.create_reduce_group(reduce_tag, tags[0], {tags.begin(), tags.end()});
    // auto group_fut2 = job.create_reduce_group(reduce_tag, tags[1], {tags.begin(), tags.end()});
    // REQUIRE(group_fut1.wait_for(wait_time));
    // REQUIRE(group_fut2.wait_for(wait_time));
    // auto& group_handle1 = group_fut1.get();
    // auto& group_handle2 = group_fut2.get();
    // auto value_fut1 = group_handle1.allreduce(reduce_op, 2);
    // auto value_fut2 = group_handle2.allreduce(reduce_op, 5);
    // REQUIRE(value_fut1.wait_for(wait_time));
    // REQUIRE(value_fut2.wait_for(wait_time));
    // const auto value1 = value_fut1.get();
    // const auto value2 = value_fut2.get();
    // REQUIRE(value1 == value2);
    // REQUIRE(value1 == 7);
  });

  base_manager.run();
}
