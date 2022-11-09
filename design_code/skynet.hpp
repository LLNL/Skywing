namespace skywing
{

struct tag {
  std::string job_id;
  int id;
};

struct ec {
  static ec nil;
  static ec throw_on_err;
  static ec terminate_on_err;
  static ec signal_on_err(int signal);

  bool operator==();
  bool operator!=();
  ec_type type;
};

struct timeout {
  std::chrono::duration expiration;
  timeout& operator=(std::chrono::duration);
  std::chrono::time_point mark() const;
};

template<typename MessageT>
class future {
  /* constructor?? */
  void wait(ec& e);
  void wait(ec& e, timeout t);
  bool poll();
  MessageT get();
};

class instance
{
  job& get_job(); // RPC with skywing job manager on same device
}

class job {
  tag tags(int id) const;
  // // ?? {
  // void publish
  // void subscribe(callback<T>)
  // void spin();
  // void spin_once();
  // // } ??
};

namespace broadcast
{
template<typename MessageT>
skywing::future<void> send(MessageT val, tag t);

template<typename MessageT>
skywing::future<MessageT> receive(tag t);
} // namespace broadcast;

namespace operators
{
struct operator {};
struct add : operator {};
struct multiply : operator {};
} // namespace operators
using namespace operators;

template<typename MessageT>
skywing::future<MessageT> reduce(MessageT val, operator op);

} // namespace skywing
