#include <catch2/catch.hpp>

#include "skywing_core/skywing.hpp"

#include "utils.hpp"

#include <array>
#include <atomic>
#include <functional>

using namespace skywing;

constexpr int num_machines = 5;
constexpr int num_connections = 1;
const std::uint16_t base_port = get_starting_port();

using ValueTag = ReduceValueTag<std::int32_t>;

const std::array<ValueTag, num_machines> tags{
  ValueTag{"Tag 0"}, ValueTag{"Tag 1"}, ValueTag{"Tag 2"}, ValueTag{"Tag 3"}, ValueTag{"Tag 4"}};

const ReduceGroupTag<std::int32_t> reduce_tag{"reduce op"};

std::atomic<int> counter = 0;

std::mutex catch_mutex;

// This wasn't working with a reference, so just use a pointer
void machine_task(const NetworkInfo* const info, const int index)
{
  using namespace std::chrono_literals;
  Manager base_manager{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};

  base_manager.submit_job("job", [&](Job& the_job, ManagerHandle manager) {
    connect_network(*info, manager, index, [](ManagerHandle& m, const int i) {
      return m.connect_to_server("127.0.0.1", base_port + i).get();
    });
    // Create the reduce group
    auto fut = the_job.create_reduce_group(reduce_tag, tags[index], {tags.begin(), tags.end()});
    auto& group = fut.get();

    auto reduce1 = group.allreduce(std::plus<>{}, 1);
    ++counter;
    const auto result1 = reduce1.get();
    // Wait a while for the disconnection message to propagate so it doesn't reach
    // here after the group has already been rebuilt
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    group.rebuild().wait();
    const auto result2 = group.allreduce(std::plus<>{}, 1).get();
    std::lock_guard lock{catch_mutex};
    REQUIRE_FALSE(result1);
    REQUIRE(result2);
    REQUIRE(*result2 == num_machines);
  });
  base_manager.run();
}

TEST_CASE("Reduce works", "[Skywing_SimpleReduce]")
{
  const auto network_info = make_network(num_machines, num_connections);
  std::vector<std::thread> threads;
  for (auto i = 1; i < num_machines; ++i) {
    threads.emplace_back(machine_task, &network_info, i);
  }
  for (auto i = 0; i < 2; ++i) {
    // Wait a bit to allow machines to process the disconnection
    if (i == 1) { std::this_thread::sleep_for(std::chrono::milliseconds{100}); }
    // Have to be the 0-th machine because it will form any connections
    // needed as lower numbered machines connect to higher numbered ones
    const auto index = 0;
    Manager base_manager{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
    base_manager.submit_job("job", [&](Job& the_job, ManagerHandle manager) {
      connect_network(network_info, manager, index, [](ManagerHandle& m, const int i) {
        return m.connect_to_server("127.0.0.1", base_port + i).get();
      });
      auto fut = the_job.create_reduce_group(reduce_tag, tags[index], {tags.begin(), tags.end()});
      auto& group = fut.get();

      while (counter != static_cast<int>(num_machines - 1)) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
      // Only do the reduce on the second go-around
      if (i == 1) {
        auto reduce = group.allreduce(std::plus<>{}, 1);
        const auto result = reduce.get();
        std::lock_guard lock{catch_mutex};
        REQUIRE(result);
        REQUIRE(*result == num_machines);
      }
    });
    base_manager.run();
  }
  for (auto&& thread : threads) {
    thread.join();
  }
}