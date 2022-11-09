#ifndef IDEMPOTENT_PROCESSOR_HPP
#define IDEMPOTENT_PROCESSOR_HPP

using namespace skywing;

/**
 * An idempotent operation is one that can be applied multiple times
 * without changing the result. This processor can be used to
 * collectively compute operations in which the local update is
 * idempotent. 
 *
 * Some examples include maximum, minimum, unions, logical-AND,
 * logical-OR, and projections. This does not include operations such
 * as summation and averaging.
 *
 * @tparam T The data type.
 * @tparam BinaryOperation The idempotent operation.
 */
template<typename T, typename BinaryOperation>
class IdempotentProcessor
{
public:
  using ValueType = T;

  IdempotentProcessor(T starting_value)
    : curr_value_(starting_value)
  { }

  IdempotentProcessor(BinaryOperation op,
                      T starting_value)
    : op_(std::move(op)), curr_value_(starting_value)
  { }

  ValueType get_init_publish_values()
  { return curr_value_; }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler, const IterMethod&)
  {
    curr_value_ = op_
      (curr_value_,
       nbr_data_handler.template f_accumulate<T>([](const T& t){return t;}, op_));
  }

  ValueType prepare_for_publication(ValueType)
  { return curr_value_; }

  T get_value() const { return curr_value_; }

private:
  T curr_value_;
  BinaryOperation op_;
};

template<typename T, typename Selector>
struct SelectionOp
{
  Selector selector_;
  T operator()(const T& t1, const T& t2) { return selector_(t1, t2) ? t1 : t2; }
}; // struct SelectionOp

template<typename T>
using MaxProcessor = IdempotentProcessor<T, SelectionOp<T, std::greater<T>>>;

template<typename T>
using MinProcessor = IdempotentProcessor<T, SelectionOp<T, std::less<T>>>;

template<typename T>
using LogicalAndProcessor = IdempotentProcessor<T, std::logical_and<T>>;

template<typename T>
using LogicalOrProcessor = IdempotentProcessor<T, std::logical_or<T>>;


#endif // IDEMPOTENT_PROCESSOR_HPP
