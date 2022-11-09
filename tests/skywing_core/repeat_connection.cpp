#include <catch2/catch.hpp>

#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"

#include "utils.hpp"

#include <chrono>
#include <iostream>
#include <string>

using namespace skywing;

constexpr int num_machines = 3;
constexpr int num_conns = 10;
constexpr int max_conn_attempts = 5;
constexpr std::chrono::milliseconds wait_period{200};

const std::uint16_t base_port = get_starting_port();

bool try_conn(ManagerHandle manager, int connecting_index)
{
  for (int i = 0; i < max_conn_attempts; ++i) {
    if (manager.connect_to_server("localhost", base_port + connecting_index).get()) { return true; }
    std::this_thread::sleep_for(wait_period);
  }
  return false;
}

void machine_task(const int index)
{
  Manager base_manager{
    static_cast<std::uint16_t>(base_port + index), std::to_string(index), std::chrono::milliseconds{100}};
  base_manager.submit_job("job", [&](Job&, ManagerHandle manager) {
    std::ranlux48 prng{std::random_device{}()};
    for (int i = 0; i < num_conns; ++i) {
      std::uniform_int_distribution<int> dist{0, num_machines - 1};
      // This feels ugly but not really worth thinking about how to fix
      auto conn_index = dist(prng);
      while (conn_index == index) {
        conn_index = dist(prng);
      }
      REQUIRE(try_conn(manager, conn_index));
    }
    SKYNET_SYNCHRONIZE_MACHINES(num_machines);
  });
  base_manager.run();
}

TEST_CASE("Heartbeats are sent", "[Heartbeat_basic]")
{
  using namespace std::chrono_literals;
  std::vector<std::thread> threads;
  for (int i = 0; i < num_machines; ++i) {
    threads.emplace_back(machine_task, i);
  }
  for (auto&& thread : threads) {
    thread.join();
  }
}
