#include "skywing_core/skywing.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/sum_processor.hpp"
#include "skywing_mid/power_method_processor.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/associative_vector.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace skywing;
using ValueTag = skywing::PublishTag<std::vector<double>>;

// First three functions are for the Skywing setup step.
std::vector<std::string> obtain_machine_names(std::uint16_t size_of_system)
{
  std::vector<std::string > machine_names;
  machine_names.resize(size_of_system);
  for(int i = 0 ; i < size_of_system; i++)
  {
    machine_names[i] = "node" + std::to_string(i+1);
  }
return machine_names;
}

std::vector<std::uint16_t>  set_port(std::uint16_t starting_port_number, std::uint16_t size_of_system)
{
  std::vector<std::uint16_t> ports;

  for(std::uint16_t i = 0; i < size_of_system; i++)
  {
    ports.push_back(starting_port_number + (i * 1));
  }
  return ports;
}

std::vector<std::string> obtain_tag_ids(int size_of_system)
{
  std::vector<std::string> tags;
  for(int i = 0; i < size_of_system; i++)
  {
      std::string hold = "push_sum_tag" +  std::to_string(i);
      tags.push_back(hold);
  }
  return tags;
}

// For this example, the exact average can be computed by inputting the system size.
double obtain_exact_average(int size_of_system)
{
  double average = 0.0;
  for(int i = 0 ; i < size_of_system; i++)
  {
    average += 1.0*i + 1.0;
  }
  average /= size_of_system;
  return average;
}

void machine_task(int machine_number, int size_of_system,
                  int number_of_neighbors,
                  std::vector<std::uint16_t> ports,
                  std::vector<std::string> machine_names,
                  std::string pubTagID, std::vector<std::string> tagIDs)
{
  skywing::Manager manager{ports[machine_number], machine_names[machine_number]};

  manager.submit_job("job", [&](skywing::Job& job, ManagerHandle manager_handle){

  if (machine_number != static_cast<int>((ports.size()) - 1) )
  {
    // Connecting to the server is an asynchronous operation and can fail.
    // Wait for the result each time and keep attempting to connect until it does
    while (!manager_handle.connect_to_server("127.0.0.1", ports[machine_number + 1]).get())
    {
      // Empty
    }
  }

  (void)number_of_neighbors;
  size_t i = static_cast<size_t>(machine_number);
  using MyAssocVec = AssociativeVector<std::uint32_t, double, false>;
  MyAssocVec matrix_column = [&]()
    {
      if (i == 0) return MyAssocVec({{i, 0.1}, {i+1, 1}});
      else if (i == static_cast<size_t>(size_of_system-1))
        return MyAssocVec({{i-1, 1}, {i, 0.1}});
      else
        return MyAssocVec({{i-1, 1}, {i, 0.1}, {i+1, 1}});
    }();
  // MyAssocVec matrix_column = [&]()
  //   {
  //     if (i == 0) return MyAssocVec({{0, 0.1}, {1, 1}, {2, 0}, {3, 0}});
  //     if (i == 1) return MyAssocVec({{0, 1}, {1, 0.1}, {2, 1}, {3, 0}});
  //     if (i == 2) return MyAssocVec({{0, 0}, {1, 1}, {2, 0.1}, {3, 1}});
  //     else return MyAssocVec({{0, 0}, {1, 0}, {2, 1}, {3, 0.1}});
  //   }();

  // make gossip connections in a circle
  size_t sos = static_cast<size_t>(size_of_system);
  std::vector<std::string> tagIDs_for_sub = [&]()
    {
      if (i == 0)
        return std::vector<std::string>({tagIDs[i], tagIDs[i+1], tagIDs[sos-1]});
      else if (i == sos-1)
        return std::vector<std::string>({tagIDs[0], tagIDs[i-1], tagIDs[i]});
      else return std::vector<std::string>({tagIDs[i-1], tagIDs[i], tagIDs[i+1]});
    }();
  // tagIDs_for_sub.push_back(tagIDs[(i + (sos/2)) % sos]);

  // using SumMethod
  //   = SumProcessor<MyAssocVec, PushFlowProcessor<MyAssocVec>>;
  using PowerMethod = PowerMethodProcessor<std::uint32_t, double>;
  using IterMethod = AsynchronousIterative
    <PowerMethod, AlwaysPublish, StopAfterTime, TrivialResiliencePolicy>;
  Waiter<IterMethod> iter_waiter =
    WaiterBuilder<IterMethod>(manager_handle, job, pubTagID, tagIDs_for_sub)
    .set_processor(matrix_column, i)
    .set_publish_policy()
    .set_stop_policy(std::chrono::seconds(180))
    .set_resilience_policy()
    .build_waiter();
  
  IterMethod mean_or_sum = iter_waiter.get();

  mean_or_sum.run(
      [&](const decltype(mean_or_sum)& p)
      {
        std::cout << p.run_time().count() << "ms: Machine " << machine_number
                  << " has eigenvalue " << p.get_processor().get_eigenvalue()
                  << " and eigenvector element " << p.get_processor().get_eigenvector_element() << std::endl;
      } );

  (void)job;
  (void)pubTagID;
  std::this_thread::sleep_for(std::chrono::seconds(10));
  });
  manager.run();
}

int main(int argc, char* argv[])
{
  // Error checking for the number of arguments
  if (argc < 4)
  {
    std::cout << "Usage: Note Enough Arguments: " << argc << std::endl;
    return 1;
  }
  // Parse the machine number, starting_port_number, and size_of_system that was passed in
  int machine_number = std::stoi(argv[1]);
  std::uint16_t starting_port_number = std::stoi(argv[2]);
  int size_of_system = std::stoi(argv[3]);
  if(machine_number > size_of_system - 1  || machine_number < 0)
  {
    std::cerr
      << "Invalid machine_number of " << std::quoted(argv[1]) << ".\n"
      << "Must be an integer between 0 and " << size_of_system - 1 << '\n';
    return -1;
  }
  if (size_of_system <= 0)
  {
    std::cerr
      << "Invalid size_of_system of " << std::quoted(argv[1]) << ".\n"
      << "Must be an integer greater than 0 and  match the number of threads created. \n";
    return -1;
  }
  
  // Skywing setup
  std::vector<std::uint16_t> ports = set_port(starting_port_number, size_of_system);
  std::vector<std::string> machine_names = obtain_machine_names(size_of_system);
  std::vector<std::string> subTagIDs = obtain_tag_ids(size_of_system);
  // This pubTag is exists in subTags[machine_number] which is needed for initialization, but its declared separately here mainly to highlight how the creator works for the push_sum class.
  std::string pubTagID("push_sum_tag" +  std::to_string(machine_number)) ;
  // Push sum variables -> initialized by user
  int number_of_neighbors = size_of_system - 1;

  // Skywing job
  machine_task(machine_number, size_of_system, number_of_neighbors,
               ports, machine_names, pubTagID, subTagIDs);
  return 0;
}
