#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"
#include "skywing_core/manager.hpp"

#include "utils.hpp"

#include <thread>

using namespace skywing;

const std::uint16_t subscriber_port = get_starting_port();
const std::uint16_t publisher_start_port = subscriber_port + 1;
constexpr const char* publisher_id = "publisher";
constexpr const char* subscriber_id = "subscriber";

constexpr int num_values_to_publish = 5;
constexpr std::int64_t value_to_publish = 10;

using Int64Tag = PublishTag<std::int64_t>;
const Int64Tag value_tag{"value"};

std::mutex catch_mutex;
std::atomic<int> values_retrieved = 0;

void publish_once(int publish_number, std::uint16_t publish_port)
{
  // Wait to start to allow the subscriber to notice that the publisher has
  // disconnected so it won't discard this connection for re-using the id
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  Manager base_manager{publish_port, publisher_id};
  base_manager.submit_job("job", [&](Job& job, ManagerHandle manager) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    while (!manager.connect_to_server("127.0.0.1", subscriber_port).get()) { /* nothing */
    }
    job.declare_publication_intent(value_tag);
    while (manager.number_of_subscribers(value_tag) == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    job.publish(value_tag, value_to_publish);
    // Wait for the value to be retrieved
    while (values_retrieved <= publish_number) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  base_manager.run();
}

void subscriber()
{
  Manager base_manager{subscriber_port, subscriber_id};
  base_manager.submit_job("job", [&](Job& job, ManagerHandle) {
    std::cout << "Starting subscribe job" << std::endl;
    job.subscribe(value_tag).get();
    std::cout << "Subscriber finished first subscription" << std::endl;
    while (values_retrieved != num_values_to_publish) {
      std::cout << "Subscriber has received " << values_retrieved << " values" << std::endl;
      if (values_retrieved != 0) {
        // wait a bit so the publisher can disconnect
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::cout << "Subscriber about to rebuild missing tag connections" << std::endl;
        job.rebuild_missing_tag_connections().wait();
        REQUIRE(0 == 1);
        std::cout << "Subscriber finished rebuilding missing tag connections" << std::endl;
      }
      // Get value from the publisher
      std::cout << "Subscriber about to get value" << std::endl;
      const auto value = job.get_waiter(value_tag).get();
      std::cout << "Subscriber recieved value" << std::endl;
      REQUIRE(value);
      REQUIRE(*value == value_to_publish);
      ++values_retrieved;
      // Trying to get another value will always error as the publishing
      // thread will exit (then rejoin)
      std::cout << "Subscriber about to get another value from the same publisher" << std::endl;
      const auto failed_value = job.get_waiter(value_tag).get();
      std::cout << "Subscriber finished attempting to get another value from same publisher" << std::endl;
      REQUIRE_FALSE(failed_value);
    }
  });
  base_manager.run();
}

TEST_CASE("Subscribe channels breaking is fine", "[Skywing_BrokenSubscribe]")
{
  std::thread subscriber_thread{subscriber};
  for (int i = 0; i < num_values_to_publish; ++i) {
    std::cout << "Starting publisher " << i << std::endl;
    std::thread publish_thread{publish_once, i, publisher_start_port+i};
    publish_thread.join();
  }
  subscriber_thread.join();
}
