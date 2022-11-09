#ifndef SKYNET_PUBSUB_CONVERTER_HELPERS_HPP
#define SKYNET_PUBSUB_CONVERTER_HELPERS_HPP

#include "skywing_core/types.hpp"

namespace skywing
{
  // template metaprogramming structs

  /*********************************************************************
   * @brief Checks if a type is a specialization
   *********************************************************************/  

  template<typename Test, template<typename...> typename Ref>
  struct IsSpecialization : std::false_type {};

  template<template<typename...> typename Ref, typename... Args>
  struct IsSpecialization<Ref<Args...>, Ref> : std::true_type {};

  template<typename Test, template<typename...> typename Ref>
  inline constexpr bool IsSpecialization_v = IsSpecialization<Test, Ref>::value;

  /*********************************************************************
   * @brief Checks if a type is a tuple
   *********************************************************************/  

  template<typename T>
  struct IsTuple : std::false_type { };

  template<typename... Ts>
  struct IsTuple<std::tuple<Ts...>> : std::true_type { };

  template<typename T>
  inline constexpr bool IsTuple_v = IsTuple<T>::value;
  
  /*********************************************************************
   * @brief Checks if a type is a native Skywing pubsub type.
   *********************************************************************/  
  template<typename T>
  struct IsPubSubType
  {
    static constexpr bool value =
      (internal::index_of<T, PublishValueTypeList>
       != internal::size<PublishValueTypeList>);
  };
  template<typename T>
  using IsPubSubType_v = typename IsPubSubType<T>::value;
    
  /*********************************************************************
   * @brief Get the number of tuple elements, after conversion to
   * pubsub type, of I'th element of the Ts.
   *********************************************************************/
  template<std::size_t I, typename... Ts>
  struct size_of_converted
  {
    using orig_type = std::tuple_element_t<I, std::tuple<Ts...>>;
    using PSType = PubSub_t<orig_type>;
    static constexpr std::size_t size =
      std::tuple_size_v<typename TupleIfNotAlready<PSType>::tuple_type>;
  };
  template<std::size_t I, typename... Ts>
  inline constexpr std::size_t size_of_converted_v =
    size_of_converted<I, Ts...>::size;

  
  /**********************************************************************
   * @brief Get the number of tuple elements, after conversion to
   * pubsub type, of first N Ts
   **********************************************************************/
  template<int N, typename... Ts>
  struct size_of_first_N
  {
    static constexpr std::size_t size =
      size_of_converted_v<N, Ts...> + size_of_first_N<N-1, Ts...>::size;
  };
  template<typename... Ts>
  struct size_of_first_N<-1, Ts...>
  {
    static constexpr std::size_t size = 0;
  };
  template<int N, typename... Ts>
  inline constexpr std::size_t size_of_first_N_v =
    size_of_first_N<N, Ts...>::size;  


  // functions for use during conversion

  /** @brief Convert an object of type S to pubsub type and, if the
   *  result is not a tuple, wrap it in a tuple of length 1.
   */
  template<typename S>
  auto convert_and_tuplify(S t)
  {
    return TupleIfNotAlready<PubSub_t<S>>
      ::to_tuple(PubSubConverter<S>::convert(std::move(t)));
  }


  /* @brief Create a sub-tuple out of a subset of its indices.
   */
  template<typename Tuple, std::size_t... Ints>
  auto select_tuple(Tuple tuple, std::index_sequence<Ints...>)
  {
    return std::tuple<std::tuple_element_t<Ints, Tuple>...>
      (std::get<Ints>(std::forward<Tuple>(tuple))...);
  }

  /* @brief Get the sub-tuple associated with the Nth element of the
   * pre-conversion tuple.
   *
   * For example, suppose we want to send something of original type
   * \c std::tuple<T1, T2, T3>, and suppose 
   *
   * \code{.cpp}
   * PS1 = PubSubConverter<T1>::pubsub_type
   * std::tuple<PS21, PS22> = PubSubConverter<T2>::pubsub_type
   * PS3 = PubSubConverter<T3>::pubsub_type
   * \encode
   *
   * Then we have
   * \code{.cpp}
   * std::tuple<PS1, PS21, PS22, PS3> = PubSubConverter<std::tuple<T1, T2, T3>>::pubsub_type
   * \endcode
   * 
   * Now suppose we have a runtime object ps_tup of type
   * \c std::tuple<PS1, PS21, PS22, PS3>. To perform deconversion, we'll
   * need to extract each sub-tuple associated with the original
   * types. Therefore, we have
   * \code{.cpp}
   * std::tuple<PS1> = decltype(extract_Nth_tuple<0>(ps_tup));
   * std::tuple<PS21, PS22> = decltype(extract_Nth_tuple<1>(ps_tup));
   * std::tuple<PS3> = decltype(extract_Nth_tuple<2>(ps_tup));
   * \endcode
   */
  template<int I, typename PSTuple, typename... Ts>
  auto extract_Nth_tuple(PSTuple ps_tup)
  {
    constexpr std::size_t Nth_size = size_of_converted_v<I, Ts...>;
    constexpr std::size_t num_preceding = I == 0 ? 0 : size_of_first_N_v<I-1, Ts...>;
    using index_seq_for_Nth = offset_sequence_t<num_preceding, std::make_index_sequence<Nth_size>>;

    return select_tuple(ps_tup, index_seq_for_Nth{});
  }

  /* @brief If the tuple has one element, return it, otherwise return
   * the tuple.
   *
   * This is generally called immediately after \c
   * extract_Nth_tuple. Using the example in the comment from that
   * function, we have
   * \code{.cpp}
   * PS1 = decltype(remove_tuple_if_single(extract_Nth_tuple<0>(ps_tup)));
   * std::tuple<PS21, PS22> = decltype(remove_tuple_if_single(extract_Nth_tuple<1>(ps_tup)));
   * PS3 = decltype(remove_tuple_if_single(extract_Nth_tuple<2>(ps_tup)));
   * \endcode
   */
  template<typename Tuple>
  auto remove_tuple_if_single(Tuple tup)
  {
    if constexpr (std::tuple_size_v<Tuple> == 1) return std::get<0>(tup);
    else return tup;
  }
} // namespace skywing

#endif // SKYNET_PUBSUB_CONVERTER_HELPERS_HPP
