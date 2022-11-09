#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/publish_policies.hpp"

#include "utils.hpp"
#include "iterative_test_stuff.hpp"

#include <array>

using namespace skywing;

constexpr int num_machines = 3;
constexpr int num_connections = 1;

//const std::array<std::uint16_t, 3> ports{10000, 20000, 30000};
const std::uint16_t start_port = get_starting_port();
const std::array<std::uint16_t, 3> ports{
  start_port, static_cast<std::uint16_t>(start_port + 1),
    static_cast<std::uint16_t>(start_port + 2)};


std::vector<std::string> tag_ids{"tag0", "tag1", "tag2"};
//std::vector<ValueTag> tags{ValueTag{"tag0"}, ValueTag{"tag1"}, ValueTag{"tag2"}};

std::unordered_map<std::size_t, std::vector<int>> publish_values
{
  {0, std::vector<int>{0, 10}},
  {1, std::vector<int>{1, 20}},
  {2, std::vector<int>{2, 30}}
};


// int expected_result(ValueTag tag, size_t ind)
// {
//   return publish_values[tag][ind];
// }

std::mutex catch_mutex;


void machine_task(const NetworkInfo* const info, const int index)
{
  Manager base_manager{ports[index], std::to_string(index)};
  base_manager.submit_job("job", [&](Job& job_handle, ManagerHandle manager) {
    connect_network(*info, manager, index, [](ManagerHandle m, const int i) {
      return m.connect_to_server("127.0.0.1", ports[i]).get();
    });    

    using IterMethod = AsynchronousIterative<TestAsyncProcessor, AlwaysPublish, StopAfterTime, TrivialResiliencePolicy>;
    IterMethod iter_method = WaiterBuilder<IterMethod>(manager, job_handle, tag_ids[index], tag_ids)
      .set_processor(index, num_machines)
      .set_publish_policy()
      .set_stop_policy(std::chrono::seconds(5))
      .set_resilience_policy()
      .build_waiter().get();
    iter_method.run();
    
    REQUIRE(fabs(iter_method.get_processor().get_curr_average() - iter_method.get_processor().get_target()) < 0.02);
    });
  base_manager.run();
}

TEST_CASE("Asynchronous Iterative", "[Skywing_AsynchronousIterative]")
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
