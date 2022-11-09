#include "skywing_core/skywing.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/jacobi_processor.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/publish_policies.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <cstdint>
#include <fstream>
#include <type_traits>

// all jacobi_include files for matrix input and data aggregation.
#include "jacobi_data_output.hpp"

using namespace skywing;
// using ValueTag = skywing::PublishTag<std::vector<double>>;

// First three functions are for the Skywing setup step.
std::vector<std::string> obtain_machine_names(std::uint16_t size_of_network)
{
  std::vector<std::string > machine_names;
  machine_names.resize(size_of_network);
  for(int i = 0 ; i < size_of_network; i++)
  {
    machine_names[i] = "node" + std::to_string(i+1);
  }
return machine_names;
}

std::vector<std::uint16_t>  set_port(std::uint16_t starting_port_number, std::uint16_t size_of_network)
{
  std::vector<std::uint16_t> ports;

  for(std::uint16_t i = 0; i < size_of_network; i++)
  {
    ports.push_back(starting_port_number + (i * 1));
  }
  return ports;
}

std::vector<std::string> obtain_tag_ids(std::uint16_t size_of_network)
{
  std::vector<std::string> tag_ids;
  for(int i = 0; i < size_of_network; i++)
  {
    std::string hold = "tag" +  std::to_string(i);
    tag_ids.push_back(hold);
  }
  return tag_ids;
}

// All of the Skywing specific code is located in this function.
void machine_task(
    int machine_number,
    int number_of_overlapping_components,
    int trial,
    std::vector<std::vector<double>> A_partition,
    std::vector<double> b_partition,
    std::vector<double> x_partition_solution,
    std::vector<double> x_full_solution,
    std::vector<size_t> row_indices,
    std::vector<std::uint16_t> ports,
    std::vector<std::string> machine_names,
    std::vector<std::string> tag_ids,
    std::string save_directory)
{

  skywing::Manager manager{ports[machine_number], machine_names[machine_number]};

  manager.submit_job("job", [&](skywing::Job& job, ManagerHandle manager_handle) {

  if (machine_number != static_cast<int>((ports.size()) - 1) )
  {
    // Connecting to the server is an asynchronous operation and can fail.
    while (!manager_handle.connect_to_server("127.0.0.1", ports[machine_number + 1]).get())
    {
      std::cout << "Machine " << machine_number << " trying to connect to " << ports[machine_number+1] << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  std::cout << "Machine " << machine_number << " creating iteration object." << std::endl;
  
  using IterMethod = AsynchronousIterative<JacobiProcessor<double>, PublishOnLinfShift<double>,
                                           StopAfterTime, TrivialResiliencePolicy>;
  Waiter<IterMethod> iter_waiter =
    WaiterBuilder<IterMethod>(manager_handle, job, tag_ids[machine_number], tag_ids)
    .set_processor(A_partition, b_partition, row_indices)
    .set_publish_policy(1e-6)
    .set_stop_policy(std::chrono::seconds(5))
    .set_resilience_policy()
    .build_waiter();
  std::cout << "Machine " << machine_number << " about to get iteration object." << std::endl;
  IterMethod async_jacobi = iter_waiter.get();
                                       
  std::cout << "Machine " << machine_number << " about to start jacobi iteration." << std::endl;  
  async_jacobi.run([&](const decltype(async_jacobi)& p)
    {
      std::cout << p.run_time().count() << "ms: Machine " << machine_number << " has values ";
      print_vec<double>(p.get_processor().return_partition_solution());
    });
  std::cout << "Machine " << machine_number << " finished jacobi iteration." << std::endl;

  double run_time = async_jacobi.run_time().count();
  int information_received = async_jacobi.get_iteration_count();
  auto x_local_estimate = async_jacobi.get_processor().return_full_solution();
  auto x_partition_estimate = async_jacobi.get_processor().return_partition_solution();
  // Since this is a distributed algorithm, we only have access to information that allows us to have a "partial" residual, since not every agent has every row of the matrix. 
  // In contrast, we can look at error involving only the components of the solution vector x which this process updates, or it's entire estimation vector, hence "partial" versus "full" in this language and "PSQ" versus "FSQ" for "partial error squared" and "full error squared". 
  // We avoid taking square roots here in case additional post processing is wanted.
  double partial_residual = calculate_partial_residual(x_local_estimate, b_partition, A_partition);
  double partial_forward_error = calculate_partial_forward_error(row_indices, x_partition_estimate, x_partition_solution);
  double forward_error = calculate_local_forward_error( x_local_estimate, x_full_solution);

  // Saves information from each Skywing machine for post processing, if wanted.
  collect_data_each_component(machine_number, number_of_overlapping_components, trial, partial_forward_error, partial_residual, information_received, run_time, save_directory);

  std::cout << "Machine: " << machine_number << "\tNumber of Updated Components: " << row_indices.size(); 
  std::cout << std::endl;
  if(static_cast<int>(row_indices.size()) < 10)
  {
    std::cout << "\t Estimate: \t"; 
    print_vec<double>(x_partition_estimate);
    std::cout << "\t Exact Sol: \t"; 
    print_vec<double>(x_partition_solution);
  }
  std::cout << "\t FSQ Error: \t" << forward_error;
  std::cout << std::endl;
  std::cout << "\t PSQ Error: \t" << partial_forward_error;
  std::cout << std::endl;
  std::cout << "\t PSQ Residual: \t" << partial_residual;  
  std::cout << std::endl;      
  std::cout << "\t New Info: \t" << information_received ; 
  std::cout << std::endl;
  std::cout << "\t Runtime: \t" << run_time; 
  std::cout << std::endl;      
  std::cout << "\t Iteration Complete: \t" << !async_jacobi.return_iterate() ; 
  std::cout << std::endl;
  std::cout << "--------------------------------------------" << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(10));
  });
  manager.run();
}


int main(int argc, char* argv[])
{
  // Error checking for the number of arguments
  if (argc != 9)
  {
    std::cout << "Usage: Wrong Number of Arguments: " << argc << std::endl;
    return 1;
  }

  // Parse the machine number, starting_port_number, and size_of_network that was passed in
  // Do this in a lambda so that if there's an exception a dummy value can be
  // returned which will always trigger an error
    int machine_number = [&]() {
    try
    {
      return std::stoi(argv[1]);
    }
    catch (...)
    {
      return -1;
    }
  }();
  const std::uint16_t starting_port_number = [&]() {
    try
    {
      return std::stoi(argv[2]);
    }
    catch (...)
    {
      return -1;
    }
  }();
   int size_of_network = [&]() {
    try
    {
      return std::stoi(argv[3]);
    }
    catch (...)
    {
      return -1;
    }
  }();
  std::string matrix_name = [&]() {
    try
    {
      return argv[4];
    }
    catch (...)
    {
      char *hold ;
      return hold;
    }
  }();
  if (machine_number < 0 || machine_number >= size_of_network)
  {
    std::cerr
      << "Invalid machine_number of " << std::quoted(argv[1]) << ".\n"
      << "Must be an integer between 0 and " << size_of_network - 1 << '\n';
    return -1;
  }
  if (size_of_network <= 0)
  {
    std::cerr
      << "Invalid size_of_network of " << std::quoted(argv[3]) << ".\n"
      << "Must be an integer greater than 0 and  match the number of threads created. \n";
    return -1;
  }
  if(matrix_name == "")
  {
    std::cerr
      << "Linear system not specified. " << std::quoted(argv[4]) << ".\n";
    return -1;
  }

  int number_of_overlapping_components = std::stoi(argv[5]);
  std::string directory = argv[6];
  int trial = std::stoi(argv[7]);
  std::string save_directory = argv[8];
  //This creates the relevant vectors needed to interact with skywing.
  auto ports = set_port(starting_port_number, size_of_network);
  auto machine_names = obtain_machine_names(size_of_network);
  std::vector<std::string> tag_ids = obtain_tag_ids(size_of_network);

  // This collects the matrices and vectors for the function.
  std::string row_index_name= "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(number_of_overlapping_components)  + "_indices_" + matrix_name ;
  std::vector<size_t> row_indices = input_vector_from_matrix_market<size_t>(directory, row_index_name);

  std::string matrix_partition_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(number_of_overlapping_components)  + "_" + matrix_name ;
  std::vector<std::vector<double>> A_partition = input_matrix_from_matrix_market<double>(directory, matrix_partition_name);

  std::string rhs_partition_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(number_of_overlapping_components)  + "_rhs_" + matrix_name ;
  std::vector<double> b_partition = input_vector_from_matrix_market<double>(directory, rhs_partition_name);

  // Both of these vectors are contrived since we know the solution. 
  // These are not used in the jacobi class, only for data output and terminal diagnostics in the example.
  std::string x_partition_solution_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(number_of_overlapping_components)  + "_x_sol_" + matrix_name ;
  std::vector<double> x_partition_solution = input_vector_from_matrix_market<double>(directory, x_partition_solution_name);

  std::string x_sol_name =  "x_sol_" + matrix_name ;
  std::vector<double> x_full_solution = input_vector_from_matrix_market<double>("../../../examples/async_jacobi/system", x_sol_name);

  // Skywing call
  machine_task(machine_number, number_of_overlapping_components, trial, A_partition, b_partition, x_partition_solution, x_full_solution, row_indices, ports, machine_names, tag_ids, save_directory);

  return 0;
}
