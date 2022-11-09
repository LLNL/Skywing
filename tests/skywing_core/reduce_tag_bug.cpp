#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"
#include "skywing_core/skywing.hpp"

#include "utils.hpp"

#include <iostream>

using namespace skywing;

using ValueTag = ReduceValueTag<std::int32_t>;
const ReduceGroupTag<std::int32_t> reduce_tag1{"reduce op1"};
const std::vector<ValueTag> tags1{ValueTag{"tag1"}, ValueTag{"tag2"}};
const ReduceGroupTag<std::int32_t> reduce_tag2{"reduce op2"};
const std::vector<ValueTag> tags2{ValueTag{"tag3"}, ValueTag{"tag4"}};

TEST_CASE("Reduce groups with same machines work", "[Skywing_ReduceTagBug]")
{
  const auto make_task = [&](int index, std::uint16_t port) -> std::thread {
    return std::thread{[index, port]() {
      Manager manager_base{port, std::to_string(port)};
      manager_base.submit_job("job", [&](Job& job, ManagerHandle) {
        auto g1 = job.create_reduce_group(reduce_tag1, tags1[index], tags1);
        auto g2 = job.create_reduce_group(reduce_tag2, tags2[index], tags2);
        g1.wait();
        g2.wait();
      });
      manager_base.run();
    }};
  };
  const auto start_port = get_starting_port();
  make_task(0, start_port).detach();
  make_task(1, start_port + 1).detach();
  Manager manager_base{static_cast<std::uint16_t>(start_port + 2), "glue"};
  manager_base.submit_job("job", [&](Job&, ManagerHandle manager) {
    while (true) {
      if (manager.connect_to_server("127.0.0.1", start_port + 1).get()) { break; }
    }
    while (true) {
      if (manager.connect_to_server("127.0.0.1", start_port + 2).get()) { break; }
    }
    // sleep to allow information to exchange
    std::this_thread::sleep_for(std::chrono::milliseconds{500});
  });
  manager_base.run();
}
