
double do_dist_dot_product()
{
  double my_sum = ai * bi;
  auto fut = skywing::reduce(my_sum, skywing::plus)

  fut.wait(skywing::ec::throw_on_err{});

  return fut.get();
}

bool do_dist_broadcast()
{
  double my_val;
  skywing::tag tag = skywing::job::tags(27);

  auto fut = skywing::broadcast::send(my_val, tag);

  skywing::ec ec;
  fut.wait(ec);
  // send() syscalls to neighbors have completed at this point, if ec == nil

  return ec != skywing::ec::nil;
}

double get_broadcast_val()
{
  skywing::tag tag = skywing::job::tags(27);
  auto fut = skywing::broadcast::receive<double>(tag);

  using namespace std::chrono_literals;
  skywing::timeout timeout = 5ms;

  fut.wait(skywing::ec::throw_on_err{}, timeout);

  return fut.get();
}
