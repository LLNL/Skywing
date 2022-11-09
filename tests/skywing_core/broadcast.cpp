#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"
#include "skywing_core/job.hpp"
#include "skywing_core/manager.hpp"

#include "utils.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

using namespace skywing;

// Emulate multiple machines with multiple threads at this point
// The network looks like this:
//  M0   +--M1
//   |   |   |
//  M2--M3--M4
//   |       |
//   +-------+
// Higher numbered machines make connection requests to lower numbered ones

// The number of connections each machine should have when fully connected
constexpr std::array<int, 5> machine_counts{1, 2, 3, 3, 3};

// The names of the machines
constexpr std::array<const char*, 5> machine_names{"m0", "m1", "m2", "m3", "m4"};

// The names of the tags
constexpr std::array<const char*, 5> tag_names{"t0", "t1", "t2", "t3", "t4"};

// The port each machine is on
auto ports = create_ports(5);

// machine connections to make
constexpr std::array<std::array<int, 3>, 5> to_connect{
  std::array<int, 3>{-1, -1, -1},
  std::array<int, 3>{-1, -1, -1},
  std::array<int, 3>{0, -1, -1},
  std::array<int, 3>{1, 2, -1},
  std::array<int, 3>{1, 2, 3}};

using Uint64Tag = PublishTag<std::uint64_t>;

void setup_network(ManagerHandle manager, const std::size_t index)
{
  using namespace std::chrono_literals;
  // Connect to the corresponding machines (if any)
  for (const auto& machine : to_connect[index]) {
    if (machine == -1) { break; }
    while (!manager.connect_to_server("127.0.0.1", ports[machine]).get()) { /* nothing */
    }
  }
  // Wait until all machines have connected
  while (manager.number_of_neighbors() != machine_counts[index]) {
    std::this_thread::sleep_for(10ms);
  }
}

void machine_task(const std::size_t index)
{
  Manager base_manager{ports[index], machine_names[index]};
  // Submit job and broadcast on the job using each machine
  base_manager.submit_job("job 0", [index](Job& the_job, ManagerHandle manager) {
    setup_network(manager, index);
    the_job.declare_publication_intent(Uint64Tag{tag_names[index]});
    // Subscribe to everything ahead of time
    // Things trying to subscribe to each other concurrently can cause the subscription to
    // fail (and always will have that chance) so do it like this to prevent any subscriptions
    // from failing
    for (std::size_t send_index = 0; send_index < machine_counts.size(); ++send_index) {
      if (index != send_index) { the_job.subscribe(Uint64Tag{tag_names[send_index]}).wait(); }
      else {
        while (manager.number_of_subscribers(Uint64Tag{tag_names[index]})
               != static_cast<int>(machine_counts.size() - 1)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
    }
    the_job.publish(Uint64Tag{tag_names[index]}, static_cast<std::uint64_t>(index));
    for (std::size_t send_index = 0; send_index < machine_counts.size(); ++send_index) {
      if (index != send_index) {
        // Ensure thread safety
        static std::mutex m;
        std::lock_guard g{m};
        const auto val = the_job.get_waiter(Uint64Tag{tag_names[send_index]}).get();
        REQUIRE(val);
        REQUIRE(*val == send_index);
      }
    }
  });
  // Start processing messages
  base_manager.run();
}

TEST_CASE("Broadcast works", "[Skywing_Broadcast]")
{
  using namespace std::chrono_literals;
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < machine_counts.size(); ++i) {
    threads.emplace_back(machine_task, i);
  }
  for (auto&& thread : threads) {
    thread.join();
  }
}
