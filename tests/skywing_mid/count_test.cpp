#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"

#include "skywing_core/enable_logging.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/quacc_processor.hpp"

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

std::mutex catch_mutex;

void machine_task(const NetworkInfo* const info, const int index)
{
  Manager base_manager{ports[index], std::to_string(index)};
  base_manager.submit_job("job", [&](Job& job_handle, ManagerHandle manager) {
      std::cout << "Machine " << index << " about to make connections." << std::endl;
    connect_network(*info, manager, index, [](ManagerHandle m, const int i) {
      return m.connect_to_server("127.0.0.1", ports[i]).get();
    });
    std::cout << "Machine " << index << " about to build itermethod." << std::endl;
    using IterMethod = AsynchronousIterative
      <QUACCProcessor<>, AlwaysPublish, StopAfterTime, TrivialResiliencePolicy>;
    IterMethod iter_method = WaiterBuilder<IterMethod>(manager, job_handle, tag_ids[index], tag_ids)
      .set_processor(num_machines-1)
      .set_publish_policy()
      .set_stop_policy(std::chrono::seconds(3))
      .set_resilience_policy()
      .build_waiter().get();
    std::cout << "Machine " << index << " finished building itermethod." << std::endl;
    iter_method.run();
    REQUIRE(iter_method.get_processor().get_count() == num_machines);
    });
  base_manager.run();
}

TEST_CASE("Count Test", "[Skywing_CountTest]")
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
