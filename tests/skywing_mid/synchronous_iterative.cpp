#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"

#include "skywing_core/enable_logging.hpp"
#include "skywing_mid/synchronous_iterative.hpp"
#include "skywing_mid/stop_policies.hpp"

#include "utils.hpp"
#include "iterative_test_stuff.hpp"

#include <array>
#include <map>

using namespace skywing;

constexpr int num_machines = 4;
constexpr int num_connections = 1;

const std::uint16_t start_port = get_starting_port();

std::vector<std::string> tag_ids{"tag0", "tag1", "tag2", "tag3"};

const std::array<std::uint16_t, 4> ports{
  start_port, static_cast<std::uint16_t>(start_port + 1),
    static_cast<std::uint16_t>(start_port + 2), static_cast<std::uint16_t>(start_port + 3)};

std::unordered_map<std::size_t, std::vector<int>> publish_values
{
  {0, std::vector<int>{0, 10}},
  {1, std::vector<int>{1, 20}},
  {2, std::vector<int>{2, 30}},
  {3, std::vector<int>{3, 40}}
};

// const std::map<PrivateValueTag, std::vector<std::string>> nodes{
//   {private_tags[0], {"localhost:" + std::to_string(ports[0]), "localhost:" + std::to_string(ports[1])}},
//   {private_tags[1], {"localhost:" + std::to_string(ports[2]), "localhost:" + std::to_string(ports[3])}}
// };

std::mutex catch_mutex;

void machine_task(const NetworkInfo* const info, const int index)
{
  Manager base_manager{ports[index], std::to_string(index)};
  base_manager.submit_job("job", [&](Job& job_handle, ManagerHandle manager) {
    connect_network(*info, manager, index, [](ManagerHandle m, const int i) {
      return m.connect_to_server("127.0.0.1", ports[i]).get();
    });
    ///////////////////////////////
    // Normal iterative method
    ///////////////////////////////
    using IterMethod = SynchronousIterative<TestAsyncProcessor, StopAfterTime, TrivialResiliencePolicy>;
    IterMethod iter_method = WaiterBuilder<IterMethod>(manager, job_handle, tag_ids[index], tag_ids)
      .set_processor(index, num_machines)
      .set_stop_policy(std::chrono::seconds(5))
      .set_resilience_policy()
      .build_waiter().get();
    iter_method.run();
    REQUIRE(fabs(iter_method.get_processor().get_curr_average() - iter_method.get_processor().get_target()) < 0.02);
    });
  base_manager.run();
}

TEST_CASE("Synchronous Iterative", "[Skywing_SynchronousIterative]")
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
