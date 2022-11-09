#include "skywing_core/skywing.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>

// The names for each instance that is in this network
// In this program, the network consists of just 5 machines
// All names throughout a network must be unique
constexpr std::array<const char*, 5> node_names{"node1", "node2", "node3", "node4", "node5"};

// The port that each instance uses
// All connections in this program are done locally
// In addition to the base port, a second port that is 100 higher than the base
// port is also used for the publication channels
constexpr std::array<std::uint16_t, node_names.size()> node_ports{10000, 11000, 12000, 13000, 14000};

// Tag that can be used to send a value for a reduce operation
// This tag uses a signed 32-bit integer for the value to send
using I32ValueTag = skywing::ReduceValueTag<std::int32_t>;

// Tag that is used for creating reduction groups that can perform reduce operations
// The type used for this tag must match the type used for the value tags
using I32GroupTag = skywing::ReduceGroupTag<std::int32_t>;

// The tags that will be participating in the reduce group
// Each machine can only produce one tag for the reduce group, but all
// of them must know about all tags that are being used for the group
const std::vector<I32ValueTag> reduce_group_tags{
  I32ValueTag{"tag1"}, I32ValueTag{"tag2"}, I32ValueTag{"tag3"}, I32ValueTag{"tag4"}, I32ValueTag{"tag5"}};

// All of the Skywing specific code is located in this function.
void simulate_machine(const int machine_number)
{
  // Create a Skywing Manager; the Manager is responsible for handling communication
  // and other such supporting tasks in the background
  skywing::Manager manager{// The port that the Manager will listen for connections on
                        node_ports[machine_number],
                        // The name of the Manager, each instance in the network must have a unique name
                        node_names[machine_number]};
  // Submit work to the manager, each job must have a unique name locally, but can be
  // duplicated on other instances.  Jobs run on separate threads than the manager and
  // are intended to be where computation and user-defined tasks are done.  Any
  // callable object can be passed as a job, the only restrictions are that it must
  // be copyable and the signature must be compatible with the function signature
  // void(skywing::Job&, skywing::ManagerHandle)
  manager.submit_job("job", [&](skywing::Job& job, skywing::ManagerHandle manager_handle) {
    // Skywing currently has no way of automatically scanning for new machines while running
    // It will accept any connection requests made to it, however, and the only requirement
    // for it to function is that all nodes have paths to other nodes. To accomplish this,
    // have each instance connect to the higher numbered one, or for the highest numbered
    // one, just advance to the job so the connection can be accepted.
    // This must be done in the job, as it is an asynchronous operation, which requires
    // the manager to be running as well.
    if (machine_number != static_cast<int>(node_ports.size() - 1)) {
      // Connecting to the server is an asynchronous operation and can fail.
      // Wait for the result each time and keep attempting to connect until it does
      while (!manager_handle.connect_to_server("localhost", node_ports[machine_number + 1]).get()) {
        // Empty
      }
    }
    // Initiate the work to create a group that can perform reductions
    // As creating this group is an expensive operation with a lot of communication,
    // groups should be kept as long as they are needed and not be discarded as soon
    // as a single reduce operation is finished
    // This function also does not return the group itself, but a skywing::Waiter,
    // meaning that the work has been initiated and will be finished on a separate thread
    auto reduce_group_waiter = job.create_reduce_group(
      // The tag for the reduce group, the same group tag and tags used for the
      // reduce must be used for all instances taking part in the group, otherwise
      // it will malfunction
      I32GroupTag{"random number reduce"},
      // The tag that this machine is producing for the device, there can be multiple
      // instances that produce the same tag, but each instance can only produce a single
      // tag
      reduce_group_tags[machine_number],
      // All of the tags that are to take part in the reduce
      reduce_group_tags);
    // Retrieve the reduce group from the waiter; this will block until the group is
    // finished being created; the returned object can the be used to perform reduce
    // and allreduce operations
    auto& reduce_group = reduce_group_waiter.get();
    // For this example program, each instance will be generating random numbers
    // and then they will be summed together; these next few lines of code are just
    // setting up to do that
    std::uniform_int_distribution<std::int32_t> random_dist{50, 150};
    std::ranlux48 prng{std::random_device{}()};
    const auto min_value = static_cast<int>(random_dist.min() * node_names.size());
    const auto max_value = static_cast<int>(random_dist.max() * node_names.size());
    while (true) {
      const auto random_value = random_dist(prng);
      // Initiate the allreduce operation, the second parameter is a callable to
      // use, it can be anything that can be called that takes two parameters of
      // the proper type (in this case, std::int32_t) and returns the same type
      // Note that the reduce operation is run on the thread that handles
      // communication, and as such, should be limited to simple calculations
      auto waiter = reduce_group.allreduce(std::plus<>{}, random_value);
      // The waiter indicates that the final result of the reduction operation
      // is ready to be retrieved; so just retrieve the value.
      // It returns an optional as the reduce operation can fail due to disconnections
      const auto result_opt = waiter.get();
      if (!result_opt) {
        const auto cur_time = std::time(nullptr);
        std::cout << std::put_time(std::localtime(&cur_time), "[%F %T]") << " Reduce operation failed; exiting...\n";
        return;
      }
      else {
        const auto cur_time = std::time(nullptr);
        // The result should never fall outside of the specified range, but do a sanity
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
  // Start running the manager, this will start all submitted jobs and continue
  // running until all jobs are finished, at which point it will return
  manager.run();
}

int main(const int argc, const char* const argv[])
{
  // Error checking for the number of arguments
  if (argc != 2) {
    std::cerr << "Usage:\n" << argv[0] << " machine_index\n";
    return 1;
  }
  // Parse the machine number that was passed in
  // Do this in a lambda so that if there's an exception a dummy value can be
  // returned which will always trigger an error
  const int machine_number = [&]() {
    try {
      return std::stoi(argv[1]);
    } catch (...) {
      return -1;
    }
  }();
  // Make sure that the machine number is valid, outputting an error message if not
  if (machine_number < 0 || machine_number >= static_cast<int>(node_ports.size())) {
    std::cerr << "Invalid machine_index of " << std::quoted(argv[1]) << ".\n"
              << "Must be an integer between 0 and " << node_ports.size() - 1 << '\n';
    return -1;
  }
  // Run until an error occurs or the user kills the process
  simulate_machine(machine_number);
}
