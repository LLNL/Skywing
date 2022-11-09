#include <catch2/catch.hpp>

#include "skywing_core/enable_logging.hpp"
#include "skywing_core/skywing.hpp"

#include "utils.hpp"

#include <thread>

using namespace skywing;

using ValueTag = PrivateTag<std::int32_t>;
const ValueTag tag{"Test IP Tag"};
const std::int32_t tag_value = 10;
const std::uint16_t subscriber_port = get_starting_port();
const std::uint16_t publisher_port = subscriber_port + 1;
std::atomic<bool> ready_for_subscription = false;
std::atomic<bool> ready_for_publication = false;

void publisher()
{
  Manager manager{publisher_port, std::to_string(publisher_port)};
  manager.submit_job("publisher", [&](Job& job, ManagerHandle handle) {
    job.declare_publication_intent(tag);
    ready_for_subscription = true;
    handle.waiter_on_subscription_change([&]() { return handle.number_of_subscribers(tag) > 0; }).wait();
    while (!ready_for_publication) {
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    job.publish(tag, tag_value);
  });
  manager.run();
}

void subscriber()
{
  Manager manager{subscriber_port, std::to_string(subscriber_port)};
  manager.submit_job("subscriber", [&](Job& job, ManagerHandle) {
    while (!ready_for_subscription) {
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    job.ip_subscribe("127.0.0.1:" + std::to_string(publisher_port), tag).wait();
    ready_for_publication = true;
    const auto value = job.get_waiter(tag).get();
    REQUIRE(value);
    REQUIRE(*value == tag_value);
  });
  manager.run();
}

TEST_CASE("Subscribe to specific IP works", "[Skywing_IPSubscribe]")
{
  std::thread t1{publisher};
  std::thread t2{subscriber};
  t1.join();
  t2.join();
}
