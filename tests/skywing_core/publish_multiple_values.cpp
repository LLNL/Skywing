#include <catch2/catch.hpp>

#include "skywing_core/skywing.hpp"

#include "utils.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>

using namespace skywing;

constexpr int num_machines = 2;
const std::uint16_t base_port = get_starting_port();

using ValueTag = PublishTag<int, double>;
using NotifyTag = PublishTag<>;

constexpr auto reduce_op
  = [](const std::tuple<int, double>& lhs, const std::tuple<int, double>& rhs) noexcept -> std::tuple<int, double> {
  return std::tuple<int, double>{std::get<0>(lhs) + std::get<0>(rhs), std::get<1>(lhs) + std::get<1>(rhs)};
};

constexpr std::tuple<int, double> publish_value{10, 3.14159};
constexpr std::tuple<int, double> reduce_result = reduce_op(publish_value, publish_value);
const ValueTag tag0{"tag 0"};
const NotifyTag tag1{"tag 1"};

using ReduceTag = ReduceValueTag<int, double>;
const std::vector<ReduceTag> reduce_tags{ReduceTag{"tag 0"}, ReduceTag{"tag 1"}};

const ReduceGroupTag<int, double> reduce_group_name{"reduce"};

void machine_task(const NetworkInfo* const info, const int index)
{
  Manager base_manager{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
  base_manager.submit_job("job", [&](Job& job, ManagerHandle manager) {
    connect_network(*info, manager, index, [](ManagerHandle& m, const int i) {
      return m.connect_to_server("127.0.0.1", base_port + i).get();
    });
    if (index == 0) {
      job.subscribe(tag1).get();
      // Declare publication intent after subscribing so that the other
      // machine won't publish too early
      job.declare_publication_intent(tag0);
      job.get_waiter(tag1).get();
      job.publish(tag0, publish_value);
    }
    else {
      job.declare_publication_intent(tag1);
      job.subscribe(tag0).get();
      job.publish(tag1);
      const auto val = job.get_waiter(tag0).get();
      REQUIRE(val);
      REQUIRE(*val == publish_value);
    }
    auto& group = job.create_reduce_group(reduce_group_name, reduce_tags[index], reduce_tags).get();
    const auto value = group.allreduce(reduce_op, publish_value).get();
    static std::mutex catch_mutex;
    std::lock_guard g{catch_mutex};
    REQUIRE(value);
    REQUIRE(*value == reduce_result);
  });
  base_manager.run();
}

TEST_CASE("Publishing multiple values works", "[Skywing_MultiplePublish]")
{
  using namespace std::chrono_literals;
  const auto network_info = make_network(num_machines, 1);
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i) {
    threads.emplace_back(machine_task, &network_info, i);
  }
  for (auto&& thread : threads) {
    thread.join();
  }
}