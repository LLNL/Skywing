#ifndef BIG_FLOAT_HPP
#define BIG_FLOAT_HPP

#include <cmath>
#include <tuple>
#include <iostream>
#include "skywing_mid/pubsub_converter.hpp"

namespace skywing
{
  
class BigFloat
{
public:

  /** @brief Construct a BigFloat from another floating type.
   */
  template<typename float_t>
  BigFloat(float_t f)
  {
    int exp;
    fraction_ = std::frexp(f, &exp);
    exp_ = static_cast<std::int64_t>(exp);
  }

  BigFloat() : exp_(0), fraction_(0.0)  {}

  /** @brief Convert a BigFloat into a double.
   *
   * If the BigFloat cannot be properly converted into a double due to
   * overflow, returns +/- HUGE_VAL.
   */
  static double to_double(const BigFloat& bf)
  {
    return std::ldexp(bf.fraction_, bf.exp_);
  }

  double to_double() const { return BigFloat::to_double(*this); }

  std::tuple<std::int64_t, double> get_underlying_data() const
  { return std::make_tuple(exp_, fraction_); }

  explicit operator double() const { return BigFloat::to_double(*this); }
  
private:
  /** @brief Construct a BigFloat from an exponent and a fraction.
   */
  BigFloat(std::int64_t exp, double fraction)
  {
    // the fraction parameter may not be in the correct range, so need to adjust.
    int exp_from_frac;
    fraction_ = std::frexp(fraction, &exp_from_frac);
    if (fraction_ == 0.0) exp_ = 0;
    else exp_ = exp + exp_from_frac;
  }


  // BigFloat represents a number of the form fraction_ * 2^exp_,
  // where fraction_ is in the range (-1, -0.5], [0.5, 1.0) and exp_
  // is a signed integer.
  std::int64_t exp_;
  double fraction_;

  friend BigFloat operator+(const BigFloat& a, const BigFloat& b);
  friend BigFloat operator-(const BigFloat& a);
  friend BigFloat operator-(const BigFloat& a, const BigFloat& b);
  friend BigFloat operator*(const BigFloat& a, const BigFloat& b);
  friend BigFloat operator/(const BigFloat& a, const BigFloat& b);
  friend BigFloat operator*(double d, const BigFloat& a);
  friend BigFloat operator/(const BigFloat& a, double d);
  friend bool operator<(const BigFloat& a, const BigFloat& b);
  friend bool operator>(const BigFloat& a, const BigFloat& b);
  friend bool operator==(const BigFloat& a, const BigFloat& b);
  friend std::ostream& operator << (std::ostream &out, const BigFloat& b);
  friend BigFloat pow2(const BigFloat& input);
  friend BigFloat log2(const BigFloat& input);
  friend BigFloat exp(const BigFloat& input);
  friend BigFloat log(const BigFloat& input);
  friend class PubSubConverter<BigFloat>;
}; // class BigFloat


/**************************************************
 * Arithmetic operators
 *************************************************/
  
BigFloat operator+(const BigFloat& a, const BigFloat& b)
{
  if (a.fraction_ == 0) return b;
  if (b.fraction_ == 0) return a;
  
  double mag_diff = a.exp_ - b.exp_;
  if (mag_diff > 0)
  {
    double b_frac = b.fraction_ * pow(2.0, -mag_diff);
    return BigFloat(a.exp_, a.fraction_ + b_frac);
  }
  else if (mag_diff < 0)
  {
    double a_frac = a.fraction_ * pow(2.0, mag_diff);
    return BigFloat(b.exp_, b.fraction_ + a_frac);
  }
  else return BigFloat(a.exp_, a.fraction_ + b.fraction_);
}

BigFloat& operator+=(BigFloat& a, const BigFloat& b)
{
  a = a + b;
  return a;
}

BigFloat operator-(const BigFloat& a) {  return BigFloat(a.exp_, -a.fraction_); }

BigFloat& operator-=(BigFloat& a, const BigFloat& b)
{
  a = a - b;
  return a;
}

BigFloat operator-(const BigFloat& a, const BigFloat& b)
{ return a + (-b); }

BigFloat operator*(const BigFloat& a, const BigFloat& b)
{ return BigFloat(a.exp_ + b.exp_, a.fraction_ * b.fraction_); }

BigFloat operator/(const BigFloat& a, const BigFloat& b)
{ return BigFloat(a.exp_ - b.exp_, a.fraction_ / b.fraction_); }

BigFloat operator*(double d, const BigFloat& a)
{ return BigFloat(a.exp_, d*a.fraction_); }

BigFloat operator/(const BigFloat& a, double d)
{ return BigFloat(a.exp_, a.fraction_ / d); }

bool operator<(const BigFloat& a, const BigFloat& b)
{
  if (a.fraction_ == 0) return (0 < b.fraction_);
  if (b.fraction_ == 0) return (a.fraction_ < 0);  
  if (a.exp_ == b.exp_) return (a.fraction_ < b.fraction_);
  if ((a.fraction_ * b.fraction_) < 0) return a.fraction_ < b.fraction_;

  if (a.fraction_ > 0) return (a.exp_ < b.exp_);
  else return (a.exp_ > b.exp_);
}

bool operator>(const BigFloat& a, const BigFloat& b)
{ return b < a; }

bool operator==(const BigFloat& a, const BigFloat& b)
{ return (a.exp_ == b.exp_) && (a.fraction_ == b.fraction_); }

std::ostream& operator << (std::ostream &out, const BigFloat& b)
{
  out << b.fraction_;
  out << "*2^";
  out << b.exp_;
  return out;
}
  
/**************************************************
 * Useful functions
 *************************************************/

/** @brief Compute 2^input
 *
 * The input BigFloat must be small enough that 2^(exponent.exp_)
 * can be represented directly as a std::int64_t. Thus, input.exp_
 * must be <= 63, that is, input must be less than a quintillion or
 * so.
 */
BigFloat pow2(const BigFloat& input)
{
  // BigFloat isn't intended for super high precision, so if exp_ <
  // 0, then we're very near 1.0, just do the easy thing here.
  if (input.exp_ < 0)
    return BigFloat(std::pow(2.0, input.to_double()));

  const int MAX_EXP = 63;
  if (std::abs(input.exp_) > MAX_EXP)
    throw std::runtime_error
      ("BigFloat::pow2: Input BigFloat is too large, will overflow even BigFloat.");

  // otherwise, need to handle potentially very large over very
  // small numbers that easily overflow double.
  // need to compute
  // 2^(2^exp_ * fraction_) = (2^fraction_)^(2^exp_)
    
  // abs(input.fraction_) is between 0.5 and 1.0 so this is
  // within bounds of double, so we can use std::pow(double, double).
  BigFloat new_base(std::pow(2.0, input.fraction_));

  // new_base = 2^p * new_frac
  // so need to compute (2^p * new_frac)^(2^exp_)
  // = (2^p)^(2^exp_) * new_frac^(2^exp_)
  std::int64_t two_exp = ((std::int64_t)1) << input.exp_; // 2^exp_
  std::int64_t new_exp = new_base.exp_ * two_exp; // (2^p)^(two_exp) = 2^(two_exp * p)

  // new_frac^(2_exp_) = ((((new_frac)^2)^2)^2...) iterated exp_
  // times. Due to size restrictions on the input parameter, this
  // loop will take <= 63 iterations.
  BigFloat new_frac(new_base.fraction_); // between 0.5 and 1.0
  for (int i = 0; i < input.exp_; i++)
    new_frac = new_frac * new_frac;

  // if new_frac = 2^p * frac, then
  // Now result is 2^new_exp * new_frac = 2^new_exp * 2^p * frac
  // = 2^(new_exp + p) * frac
  return BigFloat(new_exp + new_frac.exp_, new_frac.fraction_);
}

/** @brief Compute log_2(input)
 */ 
BigFloat log2(const BigFloat& input)
{
  // log_2(2^exp_ * fraction_) = exp_ + log_2(fraction_)
  return BigFloat(static_cast<double>(input.exp_) + std::log2(input.fraction_));
}

/** @brief Compute exp(input)
 */
BigFloat exp(const BigFloat& input)
{
  // exp(input) = 2^(log_2(e) * input)
  const double LOG_2_E = std::log2(std::exp(1.0));
  return pow2(LOG_2_E * input);
}

/** @brief Compute the natural logarithm of the input.
 */
BigFloat log(const BigFloat& input)
{
  // log_e(input) = log_2(input) / log_2(e)
  const double LOG_2_E = std::log2(std::exp(1.0));
  return log2(input) / LOG_2_E;
}

/**************************************************
 * Pubsub conversion
 *************************************************/

template<>
struct PubSubConverter<BigFloat>
{
  using pubsub_type = std::tuple<std::int64_t, double>;
  static pubsub_type convert(BigFloat bf)
  {
    return std::make_tuple(bf.exp_, bf.fraction_);
  }
  static BigFloat deconvert(pubsub_type ps)
  {
    return BigFloat(std::get<0>(ps), std::get<1>(ps));
  }
}; // struct PubSubConverter<BigFloat>

} // namespace skywing
#endif // BIG_FLOAT_HPP
