#ifndef ITERATIVE_TEST_STUFF_HPP
#define ITERATIVE_TEST_STUFF_HPP

#include <cmath>

int expected_result(std::string& tag_id, size_t ind);

struct TestWaitForNbrsStopPolicy
{
  using ValueType = double;
  
  TestWaitForNbrsStopPolicy(double coef, double stop_val,
                            size_t machine_ind)
    : coef_(coef), stop_val_(stop_val), machine_ind_(machine_ind)
  {}

  ValueType get_init_publish_values()
  {
    return get_curr_val();
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler,
                      [[maybe_unused]] const IterMethod&)
  {
    ++curr_iter_;
    // gets the minimum of all neighbor values
    min_val_ = nbr_data_handler.template f_accumulate<double>
      ([](const double& d){return d;},
       [](double d1, double d2) { return d1 < d2 ? d1 : d2; });

    // if my value is less than the neighbor min, set my val to be min
    double curr_val = get_curr_val();
    std::cout << "Machine " << machine_ind_ << " in TestWaitForNbrsStopPolicy::process_update: (min nbrval, currval) = (" << min_val_ << ", " << curr_val << ")" << std::endl;
    if (curr_val < min_val_) min_val_ = curr_val;
  }

  ValueType prepare_for_publication([[maybe_unused]] ValueType vals_to_publish)
  {
    return get_curr_val();
  }

  // stop when minimum seen value goes over a threshold
  template<typename CallerT>
  bool operator()(const CallerT&)
  {
    std::cout << "Machine " << machine_ind_ << " in TestWaitForNbrsStopPolicy::operator(): (min_val, stop_val) = (" << min_val_ << ", " << stop_val_ << ")" << std::endl;
    return min_val_ > stop_val_;
  }

private:
  double get_curr_val() { return coef_ * curr_iter_; }

  double coef_;
  double stop_val_;
  size_t curr_iter_ = 0;
  double min_val_ = 999999.0;
  size_t machine_ind_;
}; // struct TestWaitForNbrsStopPolicy


class TestAsyncProcessor
{
public:
  using ValueType = double;

  TestAsyncProcessor(size_t machine_ind, size_t num_machines)
    : machine_ind_(machine_ind), num_machines_(num_machines)
  {}
  
  ValueType get_init_publish_values()
  {
    double pub_val = TARGET_VAL + (INIT_VAL - TARGET_VAL) / (1.0 + curr_iter);
    //    std::cout << "Machine " << machine_ind_ << " sending " << pub_val << std::endl;
    return pub_val;
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler,
                      [[maybe_unused]] const IterMethod&)
  {
    if (nbr_data_handler.num_neighbors() != num_machines_)
    {
      std::cerr << "Machine " << machine_ind_ << " expected " << num_machines_ << " neighbors but only have " << nbr_data_handler.num_neighbors() << std::endl;
      std::exit(1);
    }
    //    std::cout << "Machine " << machine_ind_ << " seeing sum of " << nbr_data_handler.sum() << " and " << nbr_data_handler.num_neighbors() << " neighbors" << std::endl;
    double next_avg = nbr_data_handler.average();
    REQUIRE(fabs(next_avg - TARGET_VAL) <= fabs(curr_avg - TARGET_VAL));
    curr_avg = next_avg;
    ++curr_iter;
  }

  ValueType prepare_for_publication([[maybe_unused]] ValueType vals_to_publish)
  {
    double pub_val = TARGET_VAL + (INIT_VAL - TARGET_VAL) / (1.0 + curr_iter);
    //    std::cout << "Machine " << machine_ind_ << " sending " << pub_val << std::endl;
    return pub_val;
  }

  double get_curr_average() { return curr_avg; }
  double get_target() { return TARGET_VAL; }

private:
  double INIT_VAL = 1.0;
  double TARGET_VAL = 0.0;
  size_t curr_iter = 0;
  double curr_avg = INIT_VAL;

  size_t machine_ind_;
  size_t num_machines_;
  
  friend class TestAsyncPublishPolicy;
  friend class TestAsyncStopPolicy;
}; // class TestAsyncProcessor

#endif // ITERATIVE_TEST_STUFF_HPP
