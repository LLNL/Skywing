#include "skywing_core/skywing.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "skywing_core/enable_logging.hpp"

using DataTag = skywing::PublishTag<double>;

struct MachineConfig {
  std::string name;
  std::string remote_address;
  std::vector<DataTag> tags_produced;
  std::vector<DataTag> tags_to_subscribe_to;
  std::vector<std::string> machines_to_connect_to;
  std::uint16_t port;

  template<typename T>
  static void read_until_dash(std::istream& in, std::vector<T>& read_into)
  {
    std::string temp;
    while (std::getline(in, temp)) {
      if (temp.empty()) { continue; }
      if (!in || temp.front() == '-') { break; }
      read_into.push_back(T{std::move(temp)});
    }
  }

  friend std::istream& operator>>(std::istream& in, MachineConfig& config)
  {
    std::getline(in, config.name);
    std::getline(in, config.remote_address);
    in >> config.port >> std::ws;
    read_until_dash(in, config.tags_produced);
    read_until_dash(in, config.tags_to_subscribe_to);
    read_until_dash(in, config.machines_to_connect_to);
    return in;
  }
};

// Callable signature: auto func(const double&, std::vector<double>&) -> std::pair<double, bool>
// The bool is to indicate if the function should continue iterating
template<typename Callable>
void asynchronous_iterative(
  const MachineConfig& config,
  const std::unordered_map<std::string, MachineConfig>& machines,
  const double initial_value,
  Callable act_on)
{
  skywing::Manager manager(config.port, config.name);
  if (config.tags_produced.empty()) {
    std::cerr << config.name << ": Must produce at least one tag\n";
    std::exit(1);
  }
  manager.submit_job("job", [&](skywing::Job& job, skywing::ManagerHandle manager_handle) {
    for (const auto& connect_to_name : config.machines_to_connect_to) {
      const auto conn_to_iter = machines.find(connect_to_name);
      if (conn_to_iter == machines.cend()) {
        std::cerr << "Could not find machine \"" << connect_to_name << "\" to connect to.\n";
      }
      const auto time_limit = std::chrono::steady_clock::now() + std::chrono::seconds{10};
      while (!manager_handle.connect_to_server("127.0.0.1", conn_to_iter->second.port).get()) {
        if (std::chrono::steady_clock::now() > time_limit) {
          std::cerr << config.name << ": Took too long to connect to " << conn_to_iter->second.remote_address << ":"
                    << conn_to_iter->second.port << '\n';
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
    }
    job.declare_publication_intent_range(config.tags_produced);
    // Subscribe to all the relevant tags
    auto fut = job.subscribe_range(config.tags_to_subscribe_to);
    if (!fut.wait_for(std::chrono::seconds(10))) {
      std::cerr << config.name << ": Took too long to subscribe to tags\n";
      std::exit(1);
    }
    // Cache previous values seen to feed to the callable function
    std::unordered_map<std::string, double> neighbor_values;
    double own_value = initial_value;
    job.publish(config.tags_produced.front(), own_value);
    std::ranlux48 prng{std::random_device{}()};
    while (true) {
      // Gather data from subscriptions
      for (const auto& sub_tag : config.tags_to_subscribe_to) {
        if (job.has_data(sub_tag)) { neighbor_values[sub_tag.id()] = *job.get_waiter(sub_tag).get(); }
      }
      // Only call the function if there's any data that's been seen
      if (neighbor_values.empty()) {
        // No values seen - check if all subscriptions are gone and exit if so
        const bool should_exit = [&]() {
          for (const auto& sub_tag : config.tags_to_subscribe_to) {
            if (job.tag_has_subscription(sub_tag)) { return false; }
          }
          // All tags failed
          return true;
        }();
        if (should_exit) { break; }
      }
      else {
        std::vector<double> other_values;
        std::transform(
          neighbor_values.cbegin(), neighbor_values.cend(), std::back_inserter(other_values), [](const auto& value) {
            return value.second;
          });
        bool should_exit = false;
        std::tie(own_value, should_exit) = act_on(own_value, other_values);
        job.publish(config.tags_produced.front(), own_value);
        if (should_exit) { break; }
      }
      // Sleep for a random amount of time
      const auto sleep_ms = std::uniform_int_distribution<int>{1, 5}(prng);
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
    std::cout << config.name << ": Final value is " << own_value << '\n';
  });
  manager.run();
}

int main(const int argc, const char* const argv[])
{
  // Explicitly disable logging as the output is too noisy otherwise
  SKYNET_SET_LOG_LEVEL_TO_WARN();
  if (argc != 3) {
    std::cerr << "Usage:\n" << argv[0] << " config_file machine_name\n";
    return 1;
  }
  std::ifstream fin(argv[1]);
  const char* machine_name = argv[2];
  if (!fin) {
    std::cerr << "Error opening config file \"" << argv[1] << "\"\n";
    return 1;
  }
  const std::unordered_map<std::string, MachineConfig> configurations = [&]() {
    MachineConfig temp;
    std::unordered_map<std::string, MachineConfig> to_ret;
    while (fin >> temp) {
      to_ret[temp.name] = std::move(temp);
    }
    return to_ret;
  }();
  const auto config_iter = configurations.find(machine_name);
  if (config_iter == configurations.cend()) {
    std::cerr << "Could not find configuration for machine \"" << machine_name << "\"\n";
    return 1;
  }
  // This just averages a random number on each machine, not particularly
  // useful, but I'm not sure what to do here
  auto rng = std::random_device{};
  const auto value = std::uniform_real_distribution<double>{-100, 100}(rng);
  std::cout << machine_name << ": Own value is " << value << '\n';
  asynchronous_iterative(
    config_iter->second,
    configurations,
    value,
    [iter = 0](const double& self_value, const std::vector<double>& other_values) mutable {
      constexpr int num_iters = 5'000;
      const auto new_value
        = std::accumulate(other_values.cbegin(), other_values.cend(), self_value) / (other_values.size() + 1);
      ++iter;
      return std::make_pair(new_value, iter > num_iters);
    });
}
