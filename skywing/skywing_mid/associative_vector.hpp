#ifndef ASSOCIATIVE_VECTOR_HPP
#define ASSOCIATIVE_VECTOR_HPP

#include "skywing_mid/pubsub_converter.hpp"
#include <iostream>

namespace skywing
{

template<typename index_t = std::uint32_t,
         typename val_t = double,
         bool isOpen=true>
class AssociativeVector
{
public:
  AssociativeVector(val_t default_value = 0)
    : default_value_(default_value)
  { }
  
  AssociativeVector(std::vector<index_t>&& keys, val_t default_value = 0)
    : default_value_(default_value)
  {
    for (auto it : keys)
      data_[it] = default_value_;
  }

  AssociativeVector(std::unordered_map<index_t, val_t> data,
                    val_t default_value = 0)
    : default_value_(default_value), data_(std::move(data))
  { }

  AssociativeVector(AssociativeVector<index_t, val_t, !isOpen> other)
    : default_value_(other.default_value_), data_(other.data_)
  { }

  val_t& operator[](const index_t& ind)
  {
    if constexpr (isOpen)
      {
        if (!contains(ind)) data_[ind] = default_value_;
        return data_[ind];
      }
    else
    {
      // if not, this vector is closed, so throw an error if it's not available.
      if (!contains(ind)) throw std::runtime_error("AssociativeVector::operator[] Attempted to access a nonexistent index in a closed vector.");
      return data_[ind];
    }
  }

  const val_t& at(const index_t& ind) const
  { return data_.at(ind); }

  bool contains(const index_t& ind) const
  {
    return data_.count(ind) == 1;
  }

  std::vector<index_t> get_keys() const
  {
    std::vector<index_t> keys;
    std::transform(data_.begin(), data_.end(), std::back_inserter(keys),
                   [](const typename std::unordered_map<index_t,val_t>::value_type &pair){return pair.first;});
    return keys;
  }

  val_t dot(const AssociativeVector<index_t, val_t, isOpen>& b)
  {
    val_t result = 0;
    for (auto&& iter : b.data_)
    {
      if (contains(iter.first)) result += data_.at(iter.first) * iter.second;
    }
    return result;
  }

  AssociativeVector& operator+=(const AssociativeVector<index_t, val_t, isOpen>& b)
  {
    for (auto iter : b.data_)
    {
      const index_t& ind = iter.first;
      if constexpr (isOpen)
      { // add key k to data_ if it isn't there already
        if (!contains(ind)) data_[ind] = default_value_;
        data_[ind] += b.at(ind);
      }
      else
      {
        // is !isOpen, only add on this index if it's already in data_
        if (contains(ind)) data_[ind] += b.at(ind);
      }
    }
    return *this;
  }

  AssociativeVector& operator-=(const AssociativeVector<index_t, val_t, isOpen>& b)
  {
    for (auto iter : b.data_)
    {
      const index_t& ind = iter.first;
      if constexpr (isOpen)
      { // add key k to data_ if it isn't there already
        if (!contains(ind)) data_[ind] = default_value_;
        data_[ind] -= b.at(ind);
      }
      else
      {
        // is !isOpen, only subtract on this index if it's already in data_
        if (contains(ind)) data_[ind] -= b.at(ind);
      }
    }
    return *this;
  }

  template<typename float_t>
  AssociativeVector& operator*=(float_t f)
  {
    for (auto&& iter : data_)
      iter.second *= f;
    return *this;
  }

  template<typename float_t>
  AssociativeVector& operator/=(float_t f)
  {
    for (auto&& iter : data_)
      iter.second /= f;
    return *this;
  }

  val_t get_default_value() const { return default_value_; }
  size_t size() const { return data_.size(); }
  
private:
  val_t default_value_;
  std::unordered_map<index_t, val_t> data_;

  template<typename I, typename V, bool O>
  friend AssociativeVector<I, V, O> operator-(const AssociativeVector<I, V, O>& a);
  template<typename I, typename V, bool O>
  friend std::ostream& operator<< (std::ostream &out,
                                   const AssociativeVector<I, V, O>& a);
  friend class AssociativeVector<index_t, val_t, !isOpen>;
  friend class PubSubConverter<AssociativeVector<index_t, val_t, isOpen>>;
}; // class AssociativeVector

template<typename index_t, typename val_t, bool isOpen>
AssociativeVector<index_t, val_t, isOpen>
operator+(const AssociativeVector<index_t, val_t, isOpen>& a,
          const AssociativeVector<index_t, val_t, isOpen>& b)
{
  AssociativeVector<index_t, val_t, true> c(a);
  c += b;
  return AssociativeVector<index_t, val_t, isOpen>(c);
}

template<typename index_t, typename val_t, bool isOpen>
AssociativeVector<index_t, val_t, isOpen>
operator-(const AssociativeVector<index_t, val_t, isOpen>& a,
          const AssociativeVector<index_t, val_t, isOpen>& b)
{
  AssociativeVector<index_t, val_t, true> c(a);
  c -= b;
  return AssociativeVector<index_t, val_t, isOpen>(c);
}

template<typename index_t, typename val_t, bool isOpen>
AssociativeVector<index_t, val_t, isOpen>
operator-(const AssociativeVector<index_t, val_t, isOpen>& a)
{
  AssociativeVector<index_t, val_t, isOpen> new_vec(a.data_);
  for (auto&& iter : new_vec.data_)
    iter.second = -iter.second;
  return new_vec;  
}
  
template<typename index_t, typename val_t, bool isOpen, typename float_t>
AssociativeVector<index_t, val_t, isOpen>
operator*(float_t f,
          const AssociativeVector<index_t, val_t, isOpen>& b)
{
  AssociativeVector<index_t, val_t, isOpen> c = b;
  return c *= f;
}

template<typename index_t, typename val_t, bool isOpen, typename float_t>
AssociativeVector<index_t, val_t, isOpen>
operator/(const AssociativeVector<index_t, val_t, isOpen>& b,
          float_t f)
{
  AssociativeVector<index_t, val_t, isOpen> c = b;
  return c /= f;
}

template<typename index_t, typename val_t, bool isOpen>
std::ostream& operator<< (std::ostream &out,
                          const AssociativeVector<index_t, val_t, isOpen>& a)
{
  out << "[ ";
  for (const auto& iter : a.data_)
  {
    out << "(";
    out << iter.first;
    out << ", ";
    out << iter.second;
    out << ") ";
  }
  out << "]";
  return out;
}

/**************************************************
 * Pubsub conversion
 *************************************************/

template<typename index_t, typename val_t, bool isOpen>
struct PubSubConverter<AssociativeVector<index_t, val_t, isOpen>>
{
  using input_type = AssociativeVector<index_t, val_t, isOpen>;
  using map_type = std::unordered_map<index_t, val_t>;
  using data_pubsub_t = PubSub_t<map_type>;
  using before_final_t = std::tuple<PubSub_t<val_t>, data_pubsub_t>;
  using pubsub_type = PubSub_t<before_final_t>;
  static pubsub_type convert(input_type input)
  {
    before_final_t bf
      (PubSubConverter<val_t>::convert(input.default_value_),
       PubSubConverter<std::unordered_map<index_t, val_t>>::convert(input.data_));
    return PubSubConverter<before_final_t>::convert(bf);
  }

  static input_type deconvert(pubsub_type ps_input)
  {
    before_final_t bf = PubSubConverter<before_final_t>::deconvert(ps_input);
    val_t default_value = PubSubConverter<val_t>::deconvert(std::get<0>(bf));
    map_type data = PubSubConverter<map_type>::deconvert(std::get<1>(bf));
    return AssociativeVector<index_t, val_t, isOpen>(data, default_value);
  }
}; // struct

} // namespace skywing

#endif // ASSOCIATIVE_VECTOR_HPP
