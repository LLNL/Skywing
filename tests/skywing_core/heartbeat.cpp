#include <catch2/catch.hpp>

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"

#include "utils.hpp"

#include <chrono>
#include <iostream>
#include <string>

// TODO: Come up with a better testing scheme, will probably involve
//       actually having multiple (virtual) machines to test so that
//       a machine can be killed without sending out a goodbye message

using namespace skywing;

constexpr int num_machines = 5;
constexpr std::chrono::milliseconds heartbeat_interval{100};
const std::uint16_t base_port = get_starting_port();

void machine_task(const NetworkInfo* const info, const int index)
{
  Manager base_manager{static_cast<std::uint16_t>(base_port + index), std::to_string(index), heartbeat_interval};
  base_manager.submit_job("dummy job", [&](Job&, ManagerHandle manager) {
    connect_network(*info, manager, index, [&](ManagerHandle m, const int i) {
      return m.connect_to_server("127.0.0.1", base_port + i).get();
    });
    std::this_thread::sleep_for(heartbeat_interval * 10);
  });
  base_manager.run();
}

TEST_CASE("Heartbeats are sent", "[Heartbeat_basic]")
{
  using namespace std::chrono_literals;
  const auto network_info = make_network(num_machines, maximum_connections(num_machines));
  std::vector<std::thread> threads;
  for (auto i = 0; i < num_machines; ++i) {
    threads.emplace_back(machine_task, &network_info, i);
  }
  for (auto&& thread : threads) {
    thread.join();
  }
}
