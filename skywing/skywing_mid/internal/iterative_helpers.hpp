#ifndef SKYNET_ITERATIVE_HELPERS_HPP
#define SKYNET_ITERATIVE_HELPERS_HPP

#include <tuple>
#include <unordered_map>
#include <type_traits>
#include "skywing_core/internal/tag_buffer.hpp"

namespace skywing
{
  template<typename TagType, typename T>
  using tag_map = std::unordered_map<TagType, T, skywing::internal::hash<TagType>>;

  /*************************************************************************
   * @brief struct that checks if a type is one of the ones that
   * Skywing sends natively. The list of native types is the
   * PublishValueTypeList in skywing_core/types.hpp
   **************************************************************************/

  template<typename T>
  struct IsNativeToSkywing
  {
    static constexpr bool value = internal::index_of<T, PublishValueTypeList> != internal::size<PublishValueTypeList>;
  };

  template<typename T>
  inline constexpr bool IsNativeToSkywing_v = IsNativeToSkywing<T>::value;  
  
  /*************************************************************************
   * @brief struct that wraps a type in a tuple if it isn't already a tuple.
   **************************************************************************/
  template<typename S>
  struct TupleIfNotAlready
  {
    using tuple_type = std::tuple<S>;
    static constexpr std::size_t tuple_size = 1;
    static tuple_type to_tuple(S t) { return std::tuple<S>(t); }
  };
  template<typename... Ss>
  struct TupleIfNotAlready<std::tuple<Ss...>>
  {
    using tuple_type = std::tuple<Ss...>;
    static constexpr std::size_t tuple_size = std::tuple_size_v<tuple_type>;
    static tuple_type to_tuple(tuple_type t) { return t; }
  };


  /****************************************************************
   * @brief Get the ValueType typename in a type, wrapped in a tuple
   * if not already, or an empty tuple if it doesn't exist.
   ******************************************************************/
  template<typename, typename = void>
  struct IfHasValueType
  {
    using tuple_of_value_type = std::tuple<>;
    using tuple_of_type = std::tuple<>;
  };
  
  template<typename T>
  struct IfHasValueType<T, std::void_t<typename T::ValueType>>
  {
    using tuple_of_value_type = std::tuple<typename T::ValueType>; //typename TupleIfNotAlready<typename T::ValueType>::tuple_type;
    using tuple_of_type = std::tuple<T>; //typename TupleIfNotAlready<T>::tuple_type;
  };

  /** @brief Get std::tuple<T1::ValueType, T2::ValueType, ...> if all ValueTypes are tuples.
   */
  template<typename... Ts>
  struct TupleOfValueTypes
  {
    using type = decltype(std::tuple_cat(std::declval<typename IfHasValueType<Ts>::tuple_of_value_type>()...));
  };

  template<typename... Ts>
  using TupleOfValueTypes_t = typename TupleOfValueTypes<Ts...>::type;

  /*************************************************************************
   * @brief Get the index of a type in a tuple.
   **************************************************************************/
  template<typename, typename>
  struct IndexOf {};

  // when the next type in the tuple is T, return index 0.
  template<typename T, typename... Ts>
  struct IndexOf<T, std::tuple<T, Ts...>>
  {
    static constexpr std::size_t index = 0;
  };

  // when the next type in the tuple is not T, return 1 + recurse.
  template<typename T, typename TOther, typename... Ts>
  struct IndexOf<T, std::tuple<TOther, Ts...>>
  {
    static_assert(!std::is_same_v<T, TOther>);
    static constexpr std::size_t index = 1 + IndexOf<T, std::tuple<Ts...>>::index;
  };

  /*************************************************************************
   * @brief Get a tuple of types in this, excluding those that do not
   * publish (aka do not define a ValueType).
   **************************************************************************/
  template<typename... Ts>
  struct TupleOfOnlyPublishers
  {
    using type = decltype(std::tuple_cat(std::declval<typename IfHasValueType<Ts>::tuple_of_type>()...));
  };

  
  /*************************************************************************
   * @brief Get the index of policy P in Method's list of Policies,
   * excluding those that do not publish (aka do not define a ValueType).
   **************************************************************************/
  template<typename P, typename Method>
  struct IndexInPublishers { };

  template<typename P, template<typename...> typename MethodTemp, typename... Ps>
  struct IndexInPublishers<P, MethodTemp<Ps...>>
  {
    using tuple_of_publishers = typename TupleOfOnlyPublishers<Ps...>::type;
    static constexpr std::size_t index = IndexOf<P, tuple_of_publishers>::index;
  };

  /*************************************************************************
   * @brief Pass T to a template and get the resulting type. If T is a
   * std::tuple<Ts...>, pass the Ts... to the template instead.
   **************************************************************************/
  template<typename T, template<typename...> typename Temp>
  struct UnwrapAndApply
  {
    using type = Temp<T>;
  };

  template<typename... Ts, template<typename...> typename Temp>
  struct UnwrapAndApply<std::tuple<Ts...>, Temp>
  {
    using type = Temp<Ts...>;
  };

  template<typename T, template<typename...> typename Temp>
  using UnwrapAndApply_t = typename UnwrapAndApply<T, Temp>::type;


  /*************************************************************************
   * @brief Constructs an std::index_sequence<N, N+1, ..., N+k> 
   ************************************************************************/
  template<std::size_t N, typename Seq> struct offset_sequence;
  
  template<std::size_t N, std::size_t... Ints>
  struct offset_sequence<N, std::index_sequence<Ints...>>
  {
    using type = std::index_sequence<Ints + N...>;
  };
  template<std::size_t N, typename Seq>
  using offset_sequence_t = typename offset_sequence<N, Seq>::type;

  
  /*************************************************************************
   * @brief Check if a type has a nested ValueType type.
   ************************************************************************/
  
  // primary template handles types that have no nested ::type member:
  template< class, class = void >
  struct has_ValueType : public std::false_type { };
 
  // specialization recognizes types that do have a nested ::type member:
  template< class T >
  struct has_ValueType<T, std::void_t<typename T::ValueType>> : public std::true_type { };

  template<class T>
  inline constexpr bool has_ValueType_v = has_ValueType<T>::value;
} // namespace skywing

#endif // SKYNET_ITERATIVE_HELPERS_HPP
