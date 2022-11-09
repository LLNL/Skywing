#include "skywing_core/skywing.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <thread>
#include <fstream>

using namespace skywing;

using I32ValueTag = skywing::ReduceValueTag<std::int32_t>;
using I32GroupTag = skywing::ReduceGroupTag<std::int32_t>;


struct MachineConfig
{
  std::string name;
  std::string remoteAddress;

  std::vector<std::string> serverMachineNames;
  std::uint16_t port;

  template<typename T>
  static void readUntilDash(std::istream& in, std::vector<T>& readInto)
  {
    std::string temp;
    while(std::getline(in, temp))
    {
      if (temp.empty())
      {
        continue;
      }
      if (!in || temp.front() == '-')
      {
        break;
      }
      readInto.push_back(std::move(temp));
    }
  }

  friend std::istream& operator>>(std::istream& in, MachineConfig& config)
  {
    std::getline(in, config.name);
    if (!in)
      return in; // Then the file has finished.
    std::getline(in, config.remoteAddress);
    in >> config.port >> std::ws;

    readUntilDash(in, config.serverMachineNames);
    return in;
  }
}; // struct MachineConfig


void runJob(
  const MachineConfig& config,
  const std::unordered_map<std::string, MachineConfig>& machines,
  unsigned agent_id,
  unsigned num_total_agents
)
{
  std::cout << "Agent " << config.name << " is listening on port " << config.port << std::endl;
  skywing::Manager manager(config.port, config.name);
  manager.submit_job("job", [&](skywing::Job& job, skywing::ManagerHandle managerHandle){
    std::cout << "Agent " << agent_id << " beginning the job." << std::endl;
      
    // standard connectivity boilerplate, should get a convenience function
    for (const auto& serverMachineName : config.serverMachineNames)
    {
      const auto serverMachineNameIter = machines.find(serverMachineName);
      if (serverMachineNameIter == machines.cend())
      {
        std::cerr << "Could not find machine \"" << serverMachineName << "\" to connect to.\n";
      }
      const auto timeLimit = std::chrono::steady_clock::now() + std::chrono::seconds{30};
      while (!managerHandle.connect_to_server(serverMachineNameIter->second.remoteAddress.c_str(),
                                             serverMachineNameIter->second.port).get())
      {
        if (std::chrono::steady_clock::now() > timeLimit)
        {
          std::cerr
            << config.name << ": Took too long to connect to "
              << serverMachineNameIter->second.remoteAddress
              << ":"
              << serverMachineNameIter->second.port << '\n';
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
    }

    std::cout << "Machine " << config.name << " finished connecting." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds{4});

    std::vector<I32ValueTag> reduce_group_tags;
    for (unsigned i = 0; i < num_total_agents; i++)
      reduce_group_tags.push_back(I32ValueTag{"tag" + std::to_string(i)});

    std::cout << "Agenet " << config.name << " about to create reduce group." << std::endl;
    auto reduce_group_waiter = job.create_reduce_group(
      I32GroupTag{"random number reduce"},
      reduce_group_tags[agent_id],
      reduce_group_tags);
    auto& reduce_group = reduce_group_waiter.get();
    std::cout << "Agenet " << config.name << " finished creating reduce group." << std::endl;
    std::uniform_int_distribution<std::int32_t> random_dist{50, 150};
    std::ranlux48 prng{std::random_device{}()};
    const auto min_value = static_cast<int>(random_dist.min() * num_total_agents);
    const auto max_value = static_cast<int>(random_dist.max() * num_total_agents);
    for (unsigned i = 0; i < 100; i++) {
      const auto random_value = random_dist(prng);
      auto waiter = reduce_group.allreduce(std::plus<>{}, random_value);
      const auto result_opt = waiter.get();
      if (!result_opt) {
        const auto cur_time = std::time(nullptr);
        std::cout << std::put_time(std::localtime(&cur_time), "[%F %T]") << " Reduce operation failed; exiting...\n";
        return;
      }
      else {
        const auto cur_time = std::time(nullptr);
        // The result should never fall outside of the specified range, but do axo sanity
        // check just in case
        const auto result = *result_opt;
        if (result >= min_value && result <= max_value) {
          std::cout << std::put_time(std::localtime(&cur_time), "[%F %T]") << " Allreduce summation: " << result
                    << '\n';
        }
        else {
          // If this message is ever seen and the code has otherwise been unchanged,
          // there's some kind of bug!
          std::cerr << std::put_time(std::localtime(&cur_time), "[%F %T]") << " !!! Out of range value " << result
                    << " !!!\n";
          std::exit(1);
        }
        // Sleep so there aren't tons of lines of output
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  });
  manager.run();
}

int main(const int argc, const char* const argv[])
{
  // Explicitly disable logging as the output is too noisy otherwise
  SKYNET_SET_LOG_LEVEL_TO_WARN();
  if (argc != 6)
  {
    std::cerr
      << "Usage:\n" << argv[0]
      << " config_file slurm_nodeid slurm_localid agents_per_node num_total_agents"
      << std::endl;
    return 1;
  }

  unsigned slurm_nodeid = std::stoi(argv[2]);
  unsigned slurm_localid = std::stoi(argv[3]);
  unsigned agents_per_node = std::stoi(argv[4]);
  unsigned agent_id = agents_per_node * slurm_nodeid + slurm_localid;
  std::string agent_name = "agent" + std::to_string(agent_id);
  unsigned num_total_agents = std::stoi(argv[5]);
    
  // Obtain machine configuration for *this* machine
  std::cout << "Agent name " << agent_name << " reading from " << argv[1] << std::endl;
  std::ifstream fin(argv[1]);
  if (!fin)
  {
    std::cerr << "Error opening config file \"" << argv[1] << "\"\n";
    return 1;
  }
  const std::unordered_map<std::string, MachineConfig> configurations = [&]() {
    MachineConfig temp;
    std::unordered_map<std::string, MachineConfig> toRet;
    while (fin >> temp)
    {
      toRet[temp.name] = std::move(temp);
    }
    return toRet;
  }();
  const auto configIter = configurations.find(agent_name);
  if (configIter == configurations.cend())
  {
    std::cerr << "Could not find configuration for machine \"" << agent_name << "\"\n";
    return 1;
  }

  runJob(configIter->second, configurations, agent_id, num_total_agents);
}
