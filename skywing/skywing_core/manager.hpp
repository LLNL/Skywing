#ifndef SKYNET_MANAGER_HPP
#define SKYNET_MANAGER_HPP

#include "skywing_core/internal/capn_proto_wrapper.hpp"
#include "skywing_core/internal/devices/socket_communicator.hpp"
#include "skywing_core/internal/manager_waiter_callables.hpp"
#include "skywing_core/internal/message_creators.hpp"
#include "skywing_core/internal/reduce_group.hpp"
// #include "skywing_core/basic_manager_config.hpp"
#include "skywing_core/job.hpp"
#include "skywing_core/types.hpp"

#include "gsl/span"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// This has to be separate due to requiring hashing support for the structure
namespace skywing::internal {
/** \brief Class for publisher names / addresses; would be a local structure inside
 * the manager class, but hashing support is needed
 */
struct PublisherInfo {
  std::string address;
  MachineID machine_id;

  // Hidden friend idiom - will only be found via ADL
  friend bool operator==(const PublisherInfo& lhs, const PublisherInfo& rhs) noexcept
  {
    return lhs.address == rhs.address && lhs.machine_id == rhs.machine_id;
  }
}; // struct PublisherInfo
} // namespace skywing::internal

template<>
struct std::hash<skywing::internal::PublisherInfo> {
  std::size_t operator()(const skywing::internal::PublisherInfo& i) const noexcept
  {
    return std::hash<std::string>{}(i.address) ^ std::hash<skywing::MachineID>{}(i.machine_id);
  }
}; // struct std::hash

namespace skywing {
class Manager;
class ManagerHandle;
class Job;

namespace internal {
// The default hearbeat interval
inline static constexpr std::chrono::milliseconds default_heartbeat_interval{5000};

/** \brief Tag to indicate that this connection was made by accepting a connection
 */
struct ByAccept {};

/** \brief Tag to indicate that this connection was made by requesting a connection
 */
struct ByRequest {};

/** \brief The handle used for external Skywing instances that are connected
 */
class ExternalManager {
public:
  ExternalManager(
    SocketCommunicator comm,
    const MachineID& id,
    const std::vector<MachineID>& neighbors,
    Manager& manager,
    std::uint16_t port) noexcept;

  /** \brief Handles any messages sent from the connection
   */
  void get_and_handle_messages() noexcept;

  /** \brief Sends a raw message to the other manager
   *
   * Also marks the connection as dead if any errors occur.  Does nothing
   * if the connection is marked as dead.
   */
  void send_message(const std::vector<std::byte>& c) noexcept;

  /** \brief Returns the id of the computer this is connected to
   */
  MachineID id() const noexcept;

  /** \brief Returns if the connection is dead or not
   */
  bool is_dead() const noexcept;

  /** \brief Marks the connection as dead
   */
  void mark_as_dead() noexcept;

  /** \brief Returns true if the given neighbor is present, false otherwise
   */
  bool has_neighbor(const MachineID& id) const noexcept;

  /** \brief Sends a heartbeat if enough time has passed
   */
  void send_heartbeat_if_past_interval(std::chrono::milliseconds interval) noexcept;

  /** \brief Begins the search process for the specified tags
   */
  void find_publishers_for_tags(
    const std::vector<TagID>& tags, const std::vector<std::uint8_t>& publishers_needed) noexcept;

  /** \brief The address for communication with the external manager
   */
  std::string address() const noexcept;

  /** \brief Pair version of the address
   */
  AddrPortPair address_pair() const noexcept;

  /** \brief Sets the external manager to ignore the cache on the next request
   * for publishers
   */
  void ignore_cache_on_next_request() noexcept;

  /** \brief Returns true if the external manager is subscribed to the tag,
   * returns false if it is not.
   */
  bool is_subscribed_to(const TagID& tag) const noexcept;

  /** \brief Returns true if tags should be asked for
   */
  bool should_ask_for_tags() const noexcept;

  /** \brief Returns true if there are pending tags
   */
  bool has_pending_tag_request() const noexcept;

  /** \brief Resets the backoff counter
   */
  void reset_backoff_counter() noexcept;

  /** \brief Increments the backoff counter
   */
  void increase_backoff_counter() noexcept;

  const std::vector<MachineID>& neighbors()
  { return neighbors_; }

  void add_communicator(SocketCommunicator&& comm)
  { conns_.push_back(std::move(comm)); }

private:
  // // Read some bytes from the connection, returning false if the read failed
  // bool read_from_conn(std::byte* buffer, std::size_t count) noexcept;

  // // Read some bytes from the connection, returning an empty vector if
  // // the number of bytes couldn't be read
  // std::vector<std::byte> read_from_conn(std::size_t count) noexcept;

  // Attempts to get a message
  std::optional<MessageHandler> try_to_get_message(SocketCommunicator& socket_comm) noexcept;

  // Handle status messages
  void handle_message(MessageHandler& handle) noexcept;

  // Calculate the next time tags should be requested
  std::chrono::steady_clock::time_point calc_next_request_time() const noexcept;

  // For talking with the external manager.  
  // See you'd think there would only be one SocketCommunicator for
  // talking to another agent, so why the vector? It's because
  // sometimes agents initiate connections with each other
  // simulataneously, creating multiple socket connections between the
  // same pair of agents. Deciding which one to drop would require an
  // entire agreement protocol, which isn't worth it, so just hang on
  // to both.
  std::vector<SocketCommunicator> conns_;

  // The id of the external manager
  MachineID id_;

  // The last time the machine was heard from
  std::chrono::steady_clock::time_point last_heard_;

  // The neighbors that the external machine has
  std::vector<MachineID> neighbors_;

  // The owning manager
  Manager* manager_;

  // The time that will be waited until requesting tags again
  std::chrono::steady_clock::time_point request_tags_time_;

  // Tags that the remote is subscribed for
  // std::unordered_set for fast look-up
  std::unordered_set<TagID> remote_subscriptions_;

  // The port to use to connect to the remote machine
  std::uint16_t port_;

  // The number of times requests have been unfulfilled
  std::uint8_t backoff_counter_ = 0;

  // If the next request for tags should ignore the cache or not
  bool ignore_cache_on_next_request_ = false;

  // If the connection is dead or not
  bool dead_ = false;

  // If there is a request out for tags or not
  bool pending_tag_request_ = false;
}; // class ExternalManager
} // namespace internal

/** \brief The manager Skywing instance used for communication
 */
class Manager {
public:
  friend class ManagerHandle;
  /** \brief Creates a Manager instance that listens on the specified
   * port for connections.
   *
   * \param port The port to listen on
   * \param id The ID to assign to this machine
   * \param heartbeat_interval The interval to wait between heartbeats
   */
  template<
    typename Rep = decltype(internal::default_heartbeat_interval)::rep,
    typename Period = decltype(internal::default_heartbeat_interval)::period>
  Manager(
    const std::uint16_t port,
    const MachineID& id,
    const std::chrono::duration<Rep, Period> heartbeat_interval = internal::default_heartbeat_interval) noexcept
    : Manager{port, id, std::chrono::duration_cast<std::chrono::milliseconds>(heartbeat_interval)}
  {}

  /** \brief Constructor specifically for milliseconds
   */
  Manager(const std::uint16_t port, const MachineID& id, const std::chrono::milliseconds heartbeat_interval) noexcept;

  // /** \brief Constructor for building from a file format specified in
  //  * basic_manager_config.hpp
  //  *
  //  * This will block until all of the specified connections have been made
  //  */
  // Manager(const BuildManagerInfo& info) noexcept;

  /** \brief Destructor; tells all neighbors that the device is dead
   */
  ~Manager();

  /** \brief Creates a job for the manager to execute that produces the
   * specified tags.
   *
   * Returns false if the job could not be inserted (only happens on name collision)
   */
  bool submit_job(JobID name, std::function<void(Job&, ManagerHandle)> to_run) noexcept;

  /** \brief Start running all submitted jobs
   */
  void run() noexcept;

  /** \brief Gets the id of the manager
   */
  const std::string& id() const noexcept;

  // Access for the Job class
  struct JobAccessor {
  private:
    friend class Job;

    static void
      publish(Manager& m, const VersionID version, const TagID& tag_id, gsl::span<PublishValueVariant> value) noexcept
    {
      std::lock_guard lock{m.job_mut_};
      m.publish(version, tag_id, value);
    }

    static void report_new_publish_tags(Manager& m, const std::vector<TagID>& tags) noexcept
    {
      std::lock_guard lock{m.job_mut_};
      m.report_new_publish_tags(tags);
    }

    static auto subscribe(Manager& m, const std::vector<TagID>& tag_ids) noexcept
    {
      std::lock_guard lock{m.job_mut_};
      return m.subscribe(tag_ids);
    }

    static auto create_reduce_group(Manager& m, std::unique_ptr<internal::ReduceGroupBase> group_ptr) noexcept
    {
      std::lock_guard lock{m.job_mut_};
      return m.create_reduce_group(std::move(group_ptr));
    }

    static auto ip_subscribe(Manager& m, const AddrPortPair& addr, const std::vector<TagID>& tag_ids) noexcept
    {
      std::lock_guard lock{m.job_mut_};
      return m.ip_subscribe(addr, tag_ids);
    }
  }; // struct JobAccessor

  // Accessor for the ExternalManager class
  struct ExternalManagerAccessor {
  private:
    friend class internal::ExternalManager;

    static void
      handle_get_publishers(Manager& m, const internal::GetPublishers& msg, internal::ExternalManager& from) noexcept
    {
      m.handle_get_publishers(msg, from);
    }

    static void add_publishers_and_propagate(
      Manager& m, const internal::ReportPublishers& msg, const internal::ExternalManager& from) noexcept
    {
      m.add_publishers_and_propagate(msg, from);
    }

    static bool handle_join_reduce_group(
      Manager& m, const internal::JoinReduceGroup& msg, const internal::ExternalManager& from) noexcept
    {
      return m.handle_join_reduce_group(msg, from);
    }

    static bool handle_submit_reduce_value(
      Manager& m, const internal::SubmitReduceValue& msg, const internal::ExternalManager& from) noexcept
    {
      return m.handle_submit_reduce_value(msg, from);
    }

    static bool handle_report_reduce_disconnection(
      Manager& m, const internal::ReportReduceDisconnection& msg, const internal::ExternalManager& from) noexcept
    {
      return m.handle_report_reduce_disconnection(msg, from);
    }

    static bool subscription_tags_are_produced(Manager& m, const internal::SubscriptionNotice& msg) noexcept
    {
      return m.subscription_tags_are_produced(msg);
    }

    static bool
      handle_publish_data(Manager& m, const internal::PublishData& msg, const internal::ExternalManager& from) noexcept
    {
      return m.handle_publish_data(msg, from);
    }

    static void notify_subscriptions(Manager& m) noexcept { m.notify_subscriptions_ = true; }
  }; // struct ExternalManagerAccessor

  struct ReduceGroupAccessor {
  private:
    friend class internal::ReduceGroupBase;

    static void send_reduce_data_to_parent(
      Manager& m,
      const TagID& group_id,
      const VersionID version,
      const TagID& reduce_tag,
      gsl::span<const PublishValueVariant> value) noexcept
    {
      m.send_reduce_data_to_parent(group_id, version, reduce_tag, value);
    }

    static void send_reduce_data_to_children(
      Manager& m,
      const TagID& group_id,
      const VersionID version,
      const TagID& reduce_tag,
      gsl::span<const PublishValueVariant> value) noexcept
    {
      m.send_reduce_data_to_children(group_id, version, reduce_tag, value);
    }

    static void send_report_disconnection(
      Manager& m,
      const TagID& group_id,
      const MachineID& initiating_machine,
      const ReductionDisconnectID disconnect_id) noexcept
    {
      m.send_report_disconnection(group_id, initiating_machine, disconnect_id);
    }

    static auto rebuild_reduce_group(Manager& m, const TagID& group_id) noexcept
    {
      std::lock_guard<std::mutex> lock{m.job_mut_};
      return m.rebuild_reduce_group(group_id);
    }
  }; // struct ReduceGroupAccessor

  struct WaiterAccessor {
  private:
    friend class internal::ManagerSubscribeIsDone;
    friend class internal::ManagerReduceGroupIsCreated;
    friend class internal::ManagerGetReduceGroup;
    friend class internal::ManagerConnectionIsComplete;
    friend class internal::ManagerGetConnectionSuccess;
    friend class internal::ManagerIPSubscribeComplete;
    friend class internal::ManagerIPSubscribeSuccess;

    static bool subscribe_is_done(Manager& m, const std::vector<TagID>& tags) noexcept
    {
      return m.subscribe_is_done(tags);
    }

    static bool reduce_group_is_created(Manager& m, const TagID& group_id) noexcept
    {
      return m.reduce_group_is_created(group_id);
    }

    static internal::ReduceGroupBase& get_reduce_group(Manager& m, const TagID& group_id) noexcept
    {
      return m.get_reduce_group(group_id);
    }

    static bool conn_is_complete(Manager& m, const AddrPortPair& address) noexcept
    {
      return m.conn_is_complete(address);
    }

    static bool conn_get_success(Manager& m, const AddrPortPair& address) noexcept
    {
      return m.addr_is_connected(address);
    }
  }; // struct WaiterAccessor

private:
  ///////////////////////////////////////
  // Interface for ManagerHandle
  ///////////////////////////////////////

  Waiter<bool> connect_to_server(const char* const address, const std::uint16_t port) noexcept;
  Waiter<bool> connect_to_server(std::string_view address) noexcept;
  size_t number_of_neighbors() const noexcept;
  size_t number_of_subscribers(const internal::PublishTagBase& tag) const noexcept;
  std::uint16_t port() const noexcept;

  Waiter<void> waiter_on_subscription_change(std::function<bool()> is_ready_callable) noexcept
  {
    return make_waiter(dummy_mutex_, subscription_cv_, std::move(is_ready_callable));
  }

  template<typename T>
  Waiter<T> waiter_on_subscription_change(std::function<bool()> is_ready_callable,
                                          std::function<T()> get_val_callable) noexcept
  {
    return Waiter<T>(dummy_mutex_, subscription_cv_,
                     std::move(is_ready_callable), std::move(get_val_callable));
  }


  ///////////////////////////////////////
  // End Interface for ManagerHandle
  ///////////////////////////////////////

  /** \brief See if there are any pending connections and accept them if so
   */
  void accept_pending_connections() noexcept;

  /** \brief Listens for messages from neighbors and handles them if there
   * are any.
   */
  void handle_neighbor_messages() noexcept;

  /** \brief Broadcast a message to the entire network
   *
   * \param version The message's version
   * \param tag_id The id of the tag the message is for
   * \param value The value to send
   */
  void publish(const VersionID version, const TagID& tag_id, gsl::span<PublishValueVariant> value) noexcept;

  // Adds data to the tag queue for a job from a message
  // Returns true if it was successful, false if something went wrong
  bool add_data_to_queue(const internal::PublishData& msg) noexcept;

  /** \brief Notify neighbors of a new new neighbor
   */
  void notify_of_new_neighbor(const MachineID& id) noexcept;

  /** \brief Removes all dead neighbors
   */
  void remove_dead_neighbors() noexcept;

  /** \brief Returns a vector of all the neighboring ID's
   */
  std::vector<MachineID> make_neighbor_vector() const noexcept;

  /** \brief Broadcasts a message to all neighbors that fit a criteria
   */
  template<typename Callable>
  void send_to_neighbors_if(const std::vector<std::byte>& to_send, Callable condition) noexcept
  {
    for (auto&& neighbor : neighbors_) {
      if (condition(neighbor.second)) { neighbor.second.send_message(to_send); }
    }
  }

  /** \brief Broadcasts a message to all neighbors
   */
  void send_to_neighbors(const std::vector<std::byte>& to_send) noexcept;

  // Auxillary function to help with subscribe function
  bool subscribe_is_done(const std::vector<TagID>& required_tags) const noexcept;

  /** \brief Subscribes to the passed tags.
   */
  Waiter<void> subscribe(const std::vector<TagID>& tag_ids) noexcept;

  /** \brief Subscribes to the passed tags only on a specific IP
   */
  Waiter<bool> ip_subscribe(const AddrPortPair& addr, const std::vector<TagID>& tag_ids) noexcept;

  /** \brief Handles the get_publishers message
   */
  void handle_get_publishers(const internal::GetPublishers& msg, internal::ExternalManager& from) noexcept;

  /** \brief Removes any tags that have enough publishers, returning the tags that
   * remain and the number of publishers that they need
   */
  auto remove_tags_with_enough_publishers(const internal::GetPublishers& msg) noexcept
    -> std::pair<std::vector<TagID>, std::vector<std::uint8_t>>;

  /** \brief Adds the publishers and propagate the information is required
   *
   * Returns a bool indicating if the next request for publishers should ignore the cache
   */
  void
    add_publishers_and_propagate(const internal::ReportPublishers& msg, const internal::ExternalManager& from) noexcept;

  /** \brief Produce a message containing the known publishers and tags
   */
  std::vector<std::byte> make_known_tag_publisher_message() const noexcept;

  /** \brief Reports when new tags are being produced
   */
  void report_new_publish_tags(const std::vector<TagID>& tags) noexcept;

  /** \brief Starts the process of creating a reduce group
   */
  Waiter<internal::ReduceGroupBase&> create_reduce_group(std::unique_ptr<internal::ReduceGroupBase> group_ptr) noexcept;

  /** \brief Gets a future for when a reduce group has been re-built.
   */
  Waiter<void> rebuild_reduce_group(const TagID& group_id) noexcept;

  /** \brief Returns true if the specified reduce group has been successfully created.
   *
   * "Success" in this case means that a connection with a parent and both children
   * has been established; there is no way to determine if the entire tree has been established.
   */
  bool reduce_group_is_created(const TagID& group_id) noexcept;

  /** \brief Handles a message that a child is joining a reduce group
   */
  bool handle_join_reduce_group(const internal::JoinReduceGroup& msg, const internal::ExternalManager& from) noexcept;

  /** \brief Returns a reference to a created reduce group
   *
   * \pre The reduce group exists
   */
  internal::ReduceGroupBase& get_reduce_group(const TagID& group_id) noexcept;

  /** \brief Sends a raw message to the specified ID's, removing the ID's from
   * the array if not present
   */
  void reduce_send_data_and_remove_missing(
    std::vector<MachineID>& machines, const std::vector<std::byte>& message) noexcept;

  /** \brief Sends a value for a reduce to the corresponding parents
   */
  void send_reduce_data_to_parent(
    const TagID& group_id,
    const VersionID version,
    const TagID& reduce_tag,
    gsl::span<const PublishValueVariant> value) noexcept;

  void send_reduce_data_to_children(
    const TagID& group_id,
    const VersionID version,
    const TagID& reduce_tag,
    gsl::span<const PublishValueVariant> value) noexcept;

  void send_report_disconnection(
    const TagID& group_id, const MachineID& initiating_machine, const ReductionDisconnectID disconnect_id) noexcept;

  /** \brief Handles a submit reduce value message
   */
  bool
    handle_submit_reduce_value(const internal::SubmitReduceValue& msg, const internal::ExternalManager& from) noexcept;

  /** \brief Implementation of the two above functions
   */
  bool handle_reduce_value(
    const TagID& reduce_group_id, const internal::PublishData& value, const internal::ExternalManager& from) noexcept;

  /** \brief Handle a reduce disconnect notification
   */
  bool handle_report_reduce_disconnection(
    const internal::ReportReduceDisconnection& msg, const internal::ExternalManager& from) noexcept;

  /** \brief Attempt to create connections for any pending tags.
   */
  void init_connections_for_pending_tags() noexcept;

  /** \brief Returns true if the connection to the specified address is complete
   */
  bool conn_is_complete(const AddrPortPair& address) noexcept;

  /** \brief Returns true if the connection was successful, false otherwise
   *
   * More accurately, checks if an address is currently connected, which may
   * be useful to expose at some point?
   */
  bool addr_is_connected(const AddrPortPair& address) const noexcept;

  /** \brief Process pending user requested connections
   */
  void process_pending_conns() noexcept;

  /** \brief Creates the message for the initial handshake
   */
  std::vector<std::byte> make_handshake() const noexcept;

  /** \brief Does the final steps needed for creating a reduce group
   */
  void finalize_reduce_group(const MachineID& parent_machine_id, const TagID& group_tag) noexcept;

  /** \brief Returns a reduce_tag_data_ iterator from the parent machine
   *
   * Implemented in here since it would have to be below the member variables otherwise
   */
  decltype(auto) group_from_parent_tag(const TagID& parent_tag) noexcept
  {
    // TODO: Keep a look-up map if this becomes a performance issue
    for (auto& data_pair : reduce_tag_data_) {
      const auto& group_data = data_pair.second;
      const auto& parent = internal::ReduceGroupBase::Accessor::tag_neighbors(*group_data.group).parent();
      if (parent == parent_tag) { return data_pair; }
    }
    assert(false && "No group matching the produced tag found?");
    return *reduce_tag_data_.begin();
  }

  /** \brief Returns true if the subscription tags are all produced
   */
  bool subscription_tags_are_produced(const internal::SubscriptionNotice& msg) const noexcept;

  /** \brief Handles published information
   */
  bool handle_publish_data(const internal::PublishData& msg, const internal::ExternalManager& from) noexcept;

  /** \brief Finalizes a subscription connection.
   *
   * \param tags '\0' seperated list of tags
   */
  void finalize_subscription(const std::string& tags, internal::ExternalManager& source) noexcept;

  /** \brief Asks neighbors for publishers for pending tags with no know publishers
   */
  void find_publishers_for_pending_tags(bool force_ask = false) noexcept;

  /** \brief Returns all locally produced tags as a vector
   */
  std::vector<TagID> local_tags() const noexcept;

  // For listening to connection requests
  internal::SocketCommunicator server_socket_;

  // List of the jobs that are present
  std::unordered_map<JobID, Job> jobs_;

  // List of neighboring connections
  std::unordered_map<MachineID, internal::ExternalManager> neighbors_;

  // List of publishers that are known for each tag
  std::unordered_map<TagID, std::unordered_set<internal::PublisherInfo>> publishers_for_tag_;

  // A list of tags that still need to have publishers found
  std::vector<std::string> pending_tags_;

  // Information for reduce groups, holds the tags that each group has and
  // ID for the machines that produce those tags for the group
  struct ReduceGroupData {
    explicit ReduceGroupData(std::unique_ptr<internal::ReduceGroupBase> group_ptr) noexcept
      : group{std::move(group_ptr)}
    {}

    // unique_ptr so that this is a movable type
    std::unique_ptr<internal::ReduceGroupBase> group;
    std::vector<MachineID> parent_machines;
    std::array<std::vector<MachineID>, 2> child_machines;
  };
  std::unordered_map<TagID, ReduceGroupData> reduce_tag_data_;

  // The id of this machine
  MachineID id_;

  // The time to send a heartbeat if nothing has been heard in the time
  std::chrono::milliseconds heartbeat_interval_;

  // Only allow one job access to the manager at a time
  mutable std::mutex job_mut_;

  // Dummy mutex - only used for custom waiters created by users
  mutable std::mutex dummy_mutex_;

  // List of machines that are waiting for information for producers of a certain tag
  // Uses MachineID's instead of pointers in case the remote machine disconnects and
  // the ExternalManager is deleted between the time a request is started and a response
  // is received
  // TODO: Maybe move to pointers and just make sure to remove them when the neighbor is removed?
  // Also potentially combine with tag_to_machine_ since they are tags into the same thing
  std::unordered_map<TagID, std::unordered_set<MachineID>> send_publisher_information_to_;

  // The tags that this machine produces and the self-subscription count
  std::unordered_map<TagID, int> self_sub_count_;

  // The port used for communications
  std::uint16_t port_;

  // Mapping from a machine address to a pointer to the external manager
  // This is also used for testing that a connection has completed
  std::unordered_map<AddrPortPair, internal::ExternalManager*> addr_to_machine_;

  // Mapping from a tag to the ID used for the subscription to the tag
  // Used to know when a subscription is done and for if multiple jobs
  // subscribe to the same tag
  // This is also use to mark when a pending connection is for a tag
  std::unordered_map<TagID, internal::ExternalManager*> tag_to_machine_;

  /** \brief Connection status for pending connections
   */
  enum class ConnStatus
  {
    waiting_for_conn,
    waiting_for_resp
  };

  /** \brief Type of pending connection
   */
  enum class ConnType
  {
    user_requested,
    by_accept,
    subscription,
    reduce_group,
    specific_ip
  };
  static const char* to_c_str(ConnType type) noexcept;
  // Pending connections for all types
  struct PendingInfo {
    internal::SocketCommunicator conn;
    ConnStatus status;
    ConnType type;
    std::string tag;
  };
  std::unordered_map<AddrPortPair, PendingInfo> pending_conns_;

  // Notification for when new subscriptions are created
  std::condition_variable subscription_cv_;

  // Notification for when reduce group related connections are made
  std::condition_variable reduce_group_cv_;

  // Notification for when connections are complete
  std::condition_variable connection_cv_;

  // Booleans separate for if notifications should be raised so that
  // the CV's can use notifications while the mutex is released
  bool notify_subscriptions_ = false;
  bool notify_reduce_group_ = false;
  bool notify_connection_ = false;
}; // class Manager

class ManagerHandle {
public:
  /** \brief Connects to another instance at the specified address on
   * the specified port
   *
   * \param address The address to connect to
   * \param port The port to connect on
   */
  Waiter<bool> connect_to_server(const char* const address, const std::uint16_t port) noexcept
  {
    return handle_->connect_to_server(address, port);
  }

  /** \brief Connects to another instance with the address:port format
   */
  Waiter<bool> connect_to_server(std::string_view address) noexcept
  {
    return handle_->connect_to_server(address);
  }

  /** \brief Returns the number of machines connected
   */
  int number_of_neighbors() const noexcept { return handle_->number_of_neighbors(); }

  /** \brief Returns the id of the manager
   */
  const std::string& id() const noexcept { return handle_->id(); }

  /** \brief Returns the number of subscribers that a tag has
   */
  int number_of_subscribers(const internal::PublishTagBase& tag) const noexcept
  {
    return handle_->number_of_subscribers(tag);
  }

  /** \brief Creates a waiter that has a done condition that is run anytime
   * anything with subscriptions happens
   */
  Waiter<void> waiter_on_subscription_change(std::function<bool()> is_ready_callable) noexcept
  {
    return handle_->waiter_on_subscription_change(std::move(is_ready_callable));
  }


  template<typename T>
  Waiter<T> waiter_on_subscription_change(std::function<bool()> is_ready_callable,
                                          std::function<T()> get_val_callable) noexcept
  {
    return handle_->waiter_on_subscription_change(std::move(is_ready_callable),
                                                     std::move(get_val_callable));
  }


  /** \brief Returns the port the manager is listening on
   */
  std::uint16_t port() const noexcept { return handle_->port(); }

private:
  friend class Job;

  // Private so that only jobs can create a handle
  explicit ManagerHandle(Manager& m) noexcept : handle_{&m} {}

  Manager* handle_;
}; // class ManagerHandle
} // namespace skywing

#endif // SKYNET_MANAGER_HPP
