#include <catch2/catch.hpp>

#include "skywing_core/skywing.hpp"

#include "utils.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <functional>

using namespace skywing;

constexpr int num_machines = 5;
constexpr int num_connections = 1;
const std::uint16_t base_port = get_starting_port();

using ValueTag = ReduceValueTag<std::int32_t>;

const std::array<ValueTag, num_machines> tags{
  ValueTag{"Tag 0"}, ValueTag{"Tag 1"}, ValueTag{"Tag 2"}, ValueTag{"Tag 3"}, ValueTag{"Tag 4"}};

const ReduceGroupTag<std::int32_t> reduce_tag{"reduce op"};

template<typename Group, typename Callable>
void test_reduce(Group& group, const std::int32_t value, Callable reduce_op, const std::int32_t expected_value)
{
  static std::mutex catch_mutex;
  // Normal reduce
  const auto first_result = group.reduce(reduce_op, value).get();
  // Allreduce
  const auto second_result = group.allreduce(reduce_op, value).get();
  // Wait for the value to be ready / propagated
  std::lock_guard g{catch_mutex};
  if (group.returns_value_on_reduce()) {
    REQUIRE(first_result.has_value());
    REQUIRE(*first_result == expected_value);
  }
  else {
    REQUIRE_FALSE(first_result.has_value());
    REQUIRE_FALSE(first_result.error_occurred());
  }
  REQUIRE(second_result);
  REQUIRE(*second_result == expected_value);
}

// This wasn't working with a reference, so just use a pointer
void machine_task(const NetworkInfo* const info, const int index)
{
  static std::atomic<int> counter{0};
  using namespace std::chrono_literals;
  Manager base_manager{static_cast<std::uint16_t>(base_port + index), std::to_string(index)};
  base_manager.submit_job("job", [&](Job& the_job, ManagerHandle manager) {
    connect_network(*info, manager, index, [&](ManagerHandle& m, const int i) {
      return m.connect_to_server("127.0.0.1", base_port + i).get();
    });
    // Create the reduce group
    auto fut = the_job.create_reduce_group(reduce_tag, tags[index], {tags.begin(), tags.end()});
    auto& group = fut.get();

    // Do a few reduce operations on the group
    using i32 = std::int32_t;
    test_reduce(group, index, std::plus<>{}, num_machines * (num_machines - 1) / 2);
    test_reduce(
      group, index, [](i32 a, i32 b) { return std::max(a, b); }, num_machines - 1);
    test_reduce(
      group, index, [](i32 a, i32 b) { return std::min(a, b); }, 0);
    // Due to accuracy problems, disable this test
    // test_reduce(group, index + 1, std::multiplies<>{}, static_cast<i32>(std::tgamma(num_machines + 1)));

    ++counter;
    while (counter != num_machines) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
  });
  base_manager.run();
}

TEST_CASE("Reduce works", "[Skywing_SimpleReduce]")
{
  const auto network_info = make_network(num_machines, num_connections);
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i) {
    threads.emplace_back(machine_task, &network_info, i);
  }
  for (auto&& thread : threads) {
    thread.join();
  }
}
