#ifndef SKYNET_PUBSUB_CONVERTER_HPP
#define SKYNET_PUBSUB_CONVERTER_HPP

#include <tuple>
#include <utility>
#include <type_traits>
#include "skywing_core/types.hpp"
#include "skywing_mid/internal/iterative_helpers.hpp"

namespace skywing
{
  /** @brief Convert an object into a data type that Skywing can send
   * through its pubsub system.
   *
   *  This is a default struct template that works when T is a type
   *  that Skywing can send directly, the list of which can be found in
   *  \c skywing_core/types.hpp. You should implement a specialization
   *  of this struct template that converts your non-supported types
   *  into one of the supported types or into a std::tuple of
   *  supported types.
   *
   * @tparam T The type to convert into a supported type.
   */
  template<typename T, typename Enable = void>
  struct PubSubConverter
  {
    static_assert(internal::index_of<T, PublishValueTypeList> != internal::size<PublishValueTypeList>,
                  "Looks like you want Skywing to send messages with a new data type. For Skywing to do this, you must implement a specialization of PubSubConverter<T> to translate it into something Skywing knows how to send. See skywing/skywing_mid/pubsub_converter.hpp for more information.");
    
    using input_type = T;
    using pubsub_type = T;

    /** @brief Convert a T into something Skywing's pubsub handlers
     * natively support.
     *
     * In the default, T is already of that type, so just return the
     * input.
     */
    static pubsub_type convert(T t) { return t; }

    
    /** @brief Convert a Skywing pubsub type back into an original data type.
     *
     * In the default, T is already of that type, so just return the
     * input. Note that, in general, many data types can convert to
     * the same pubsub type; for example, a list of numbers and a
     * matrix might both convert to a \c
     * std::vector<double>. Therefore, we must know the original type
     * in order to correctly deconvert.
     */
    static input_type deconvert(pubsub_type ps_t) { return ps_t; }
  }; // struct PubSubConverter

  template<typename T>
  using PubSub_t = typename PubSubConverter<T>::pubsub_type;

  /** @brief PubSubConverter specialization for \c char* types.
   */
  template<>
  struct PubSubConverter<char*>
  {
    using pubsub_type = std::string;
    static pubsub_type convert(char* c_arr) { return std::string(c_arr); }
    static char* deconvert(std::string& s) { return s.data(); }
  }; // struct PubSubConverter<char*>
} // namespace skywing

#include "skywing_mid/internal/pubsub_converter_helpers.hpp"

namespace skywing
{
  /** @brief PubSubConverter specialization for tuple types.
   *
   * Recursively applies a PubSubConverter to each type in the tuple
   * and then *flattens* nested tuples into a single-level tuple of
   * converted elements.
   *
   * E.g.
   * <tt>
   * using MyTuple = std::tuple<double, unsigned, int, std::tuple<double, char*>>;
   * using MyPSTuple = PubSub_t<MyTuple>; // has type std::tuple<double, unsigned, int, double, std::string>
   * </tt>
   */
  template<typename... Ts>
  struct PubSubConverter<std::tuple<Ts...>>
  {
    using input_type = std::tuple<Ts...>;
    using pubsub_type =
      decltype(std::tuple_cat(std::declval<typename TupleIfNotAlready<PubSub_t<Ts>>::tuple_type>()...));

    /** @brief Convert an input of type \c input_type into a tuple of type \c pusub_type.
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
     * Now suppose we have a runtime \c my_tup of type \c std::tuple<T1, T2, T3>. Then we have
     * \code{.cpp}
     * using pubsub_t = typename PubSubConverter<std::tuple<T1, T2, T3>>::pubsub_type;
     * pubsub_t my_ps_tup = PubSubConverter<std::tuple<T1, T2, T3>>::convert(my_tup);
     * \encode
     */
    template<typename Indices = std::make_index_sequence<sizeof...(Ts)>>
    static pubsub_type convert(std::tuple<Ts...> tup)
    { 
      return convert_impl(std::move(tup), Indices{});
    }

    /** @brief Convert an input of type \c pubsub_type into something of type \c input_type.
     *
     * For example, suppose we are sending and receiving information of type
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
     * Now suppose we have a runtime \c ps_tup of type \c
     * std::tuple<PS1, PS21, PS22, PS3> that we received from another
     * agent. Then we have
     * \code{.cpp}
     * using orig_type = typename PubSubConverter<std::tuple<T1, T2, T3>>::input_type; // is type std::tuple<T1, T2, T3>
     * orig_t my_tup = PubSubConverter<std::tuple<T1, T2, T3>>::deconvert(ps_tup);
     * \encode
     */
    template<typename Indices = std::make_index_sequence<sizeof...(Ts)>>
    static input_type deconvert(pubsub_type ps_tup)
    { 
      return deconvert_impl(std::move(ps_tup), Indices{});
    }

  private:
    template<std::size_t... I>
    static pubsub_type convert_impl(std::tuple<Ts...> tup, std::index_sequence<I...>)
    {
      return std::tuple_cat(convert_and_tuplify(std::move(std::get<I>(tup)))...);
    }
    

    template<std::size_t I>
    static auto extract_and_deconvert(pubsub_type& tup)
    {
      using T = std::tuple_element_t<I, std::tuple<Ts...>>;
      return PubSubConverter<T>::deconvert
        (remove_tuple_if_single(extract_Nth_tuple<I, pubsub_type, Ts...>(tup)));
    }

    template<std::size_t... I>
    static input_type deconvert_impl(pubsub_type tup, std::index_sequence<I...>)
    {
      return std::make_tuple(extract_and_deconvert<I>(tup)...);
    }
  }; // struct PubSubConverter<std::tuple<Ts...>>


  
  /** @brief PubSubConverter specialization for unordered_maps
   *
   * Converts it into a tuple of parallel vectors.
   */
  template<typename T, typename S>
  struct PubSubConverter<std::unordered_map<T, S>>
  {
    using input_type = std::unordered_map<T, S>;
    using key_type = PubSub_t<std::vector<T>>;
    using val_type = PubSub_t<std::vector<S>>;
    using before_final_t = std::tuple<key_type, val_type>;
    using pubsub_type = PubSub_t<before_final_t>;

    static pubsub_type convert(input_type in)
    {
      std::vector<T> keys_vec;
      std::vector<S> vals_vec;
      for (auto iter : in)
      {
        keys_vec.push_back(iter.first);
        vals_vec.push_back(iter.second);
      }
      return PubSubConverter<before_final_t>::convert
        (std::make_tuple(PubSubConverter<std::vector<T>>::convert(keys_vec),
                         PubSubConverter<std::vector<S>>::convert(vals_vec)));
    }

    
    static input_type deconvert(pubsub_type out)
    {
      before_final_t bf_out = PubSubConverter<before_final_t>::deconvert(out);
      
      std::vector<T> keys_vec
        = PubSubConverter<std::vector<T>>::deconvert(std::get<0>(bf_out));
      std::vector<S> vals_vec
        = PubSubConverter<std::vector<S>>::deconvert(std::get<1>(bf_out));
      input_type map_to_ret;
      for (size_t i = 0; i < keys_vec.size(); i++)
        map_to_ret[keys_vec[i]] = vals_vec[i];
      return map_to_ret;
    }
    
  }; // struct PubSubConverter<std::unordered_map<T, S>>

  /** template specialization for PubSubConverter for
   *  std::vector<std::tuple<Ts...>> when the Ts... are all native
   *  Skywing types.
   */
  template<typename... Ts>
  struct PubSubConverter<std::vector<std::tuple<Ts...>>>
  {
    using tuple_type = std::tuple<Ts...>;
    using input_type = std::vector<std::tuple<Ts...>>;
    using swapped_input_t = std::tuple<std::vector<Ts>...>;
    using before_final_t = std::tuple<PubSub_t<std::vector<Ts>>...>;
    using pubsub_type = PubSub_t<before_final_t>;

    template<typename Indices = std::make_index_sequence<sizeof...(Ts)>>
    static pubsub_type convert(std::vector<std::tuple<Ts...>> input)
    {
      return convert_impl(std::move(input), Indices{});
    }

    template<typename Indices = std::make_index_sequence<sizeof...(Ts)>>
    static input_type deconvert(pubsub_type ps_input)
    {
      return deconvert_impl(std::move(ps_input), Indices{});
    }
  private:

    template<typename S, std::size_t ind>
    static PubSub_t<std::vector<S>> build_element_vector(input_type& input)
    {
      std::vector<S> to_ret;
      for (auto it : input) to_ret.push_back(std::get<ind>(it));
      return PubSubConverter<std::vector<S>>::convert(to_ret);
    }

    template<std::size_t... I>
    static pubsub_type convert_impl(input_type input,
                                    std::index_sequence<I...>)
    {
      before_final_t bf_tup =
        std::make_tuple(build_element_vector<std::tuple_element_t<I, tuple_type>, I>(input)...);
      return PubSubConverter<before_final_t>::convert(bf_tup);
    }

    // template<typename S, std::size_t ind>
    // static S get_vector_element
    // (before_final_t& tup, std::size_t i)
    // {
    //   return PubSubConverter<std::vector<S>>::deconvert(std::get<ind>(tup)).at(i);
    //   // return PubSubConverter<S>::deconvert(std::get<ind>(tup).at(i));
    // }

    template<typename S, std::size_t ind>
    static std::vector<S> deconvert_vec(before_final_t& tup)
    {
      return PubSubConverter<std::vector<S>>::deconvert(std::get<ind>(tup));
      // return PubSubConverter<S>::deconvert(std::get<ind>(tup).at(i));
    }

    template<std::size_t... I>
    static input_type deconvert_impl(pubsub_type tup,
                                     std::index_sequence<I...>)
    {
      before_final_t bf_tup
        = PubSubConverter<before_final_t>::deconvert(tup);
      swapped_input_t swapped_to_ret
        = std::make_tuple
        (deconvert_vec<std::tuple_element_t<I, tuple_type>, I>(bf_tup)...);
      input_type to_ret;
      for (size_t i = 0; i < std::get<0>(tup).size(); i++)
      {
        to_ret.push_back
          (std::make_tuple(std::get<I>(swapped_to_ret).at(i)...));
      }
      // input_type to_ret;
      // for (size_t i = 0; i < std::get<0>(tup).size(); i++)
      // {
      //   to_ret.push_back
      //     (std::make_tuple
      //      (get_vector_element<std::tuple_element_t<I, tuple_type>, I>(bf_tup, i)...));
      // }
      return to_ret;
    }

    
  }; // struct PubSubConverter<std::vector<std::tuple<Ts...>>>


  /** @brief PubSubConverter specialization for nested vectors.
   *
   *  We unroll the vector into a single flat vector, with a vector of
   *  size_t's telling us how many elements are in each sub-vector.
   */
  template<typename T>
  struct PubSubConverter<std::vector<std::vector<T>>>
  {
    using input_type = std::vector<std::vector<T>>;
    using vecPsT_t = std::vector<PubSub_t<T>>;
    using psVecPsT_t = PubSub_t<vecPsT_t>;
    using before_final_t = std::tuple<std::vector<std::uint32_t>, psVecPsT_t>;
    using pubsub_type = PubSub_t<before_final_t>;

    static pubsub_type convert(input_type input)
    {
      std::vector<std::uint32_t> vec_sizes;
      vecPsT_t flat_vec;
      for (const auto& outerVec : input)
      {
        vec_sizes.push_back(outerVec.size());
        for (const T& elem : outerVec)
          flat_vec.push_back(PubSubConverter<T>::convert(elem));
      }
      psVecPsT_t converted_flattened_vec
        = PubSubConverter<vecPsT_t>::convert(flat_vec);
      before_final_t bf(vec_sizes, converted_flattened_vec);
      return PubSubConverter<before_final_t>::convert(bf);
    }

    static input_type deconvert(pubsub_type input)
    {
      before_final_t bf = PubSubConverter<before_final_t>::deconvert(input);
      std::vector<std::uint32_t> vec_sizes = std::get<0>(bf);
      vecPsT_t flat_vec = PubSubConverter<vecPsT_t>::deconvert(std::get<1>(bf));
      std::vector<std::vector<T>> to_ret;
      auto flat_iter = flat_vec.cbegin();
      for (const auto& i : vec_sizes)
      {
        std::vector<T> subvec;
        for (std::size_t j = 0; j < i; j++)
        {
          subvec.push_back(PubSubConverter<T>::deconvert(*flat_iter));
          ++flat_iter;
        }
        to_ret.push_back(subvec);
      }
      return to_ret;
    }
  }; // struct PubSubConverter<std::vector<std::vector<T>>

  /** @brief PubSubConverter specialization for vectors of types that
   * are not native to Skywing and that are not tuples.
   */
  template<typename T>
  struct PubSubConverter<
    std::vector<T>,
    std::enable_if_t<!IsNativeToSkywing_v<std::vector<T>>
                     && !IsTuple_v<T>
                     && !IsSpecialization_v<T, std::vector>>>
  {
    using input_type = std::vector<T>;
    using before_final_t = std::vector<PubSub_t<T>>;
    using pubsub_type = PubSub_t<before_final_t>;
      
    static pubsub_type convert(std::vector<T> input)
    {
      before_final_t before_final_vec;
      for (auto it : input)
        before_final_vec.push_back(PubSubConverter<T>::convert(it));
      return PubSubConverter<before_final_t>::convert(before_final_vec);
    }
      
    static input_type deconvert(pubsub_type ps_input)
    {
      before_final_t before_final_vec = PubSubConverter<before_final_t>::deconvert(ps_input);
      input_type to_ret;
      for (auto it : before_final_vec)
        to_ret.push_back(PubSubConverter<T>::deconvert(it));
      return to_ret;
    }
  }; // struct PubSubConverter<std::vector<T>>


  
  // struct Colin { int c; };
    
  // template<>
  // struct PubSubConverter<Colin>
  // {
  //   using pubsub_type = int;
  //   static pubsub_type convert(Colin c) { return c.c; }
  //   static Colin deconvert(pubsub_type in) { return Colin{in}; }
  // };

  // struct SirWalter { std::int64_t a; double b; };

  // template<>
  // struct PubSubConverter<SirWalter>
  // {
  //   using pubsub_type = std::tuple<std::int64_t, double>;
  //   static pubsub_type convert(SirWalter sw) { return std::make_tuple(sw.a, sw.b); }
  //   static SirWalter deconvert(pubsub_type in)
  //   { return SirWalter{std::get<0>(in), std::get<1>(in)}; }
  // };

  // void test()
  // {
  //   double d = 2.0;
  //   unsigned u = 3;

  //   char colin_arr[] = "colin";
  //   std::string colin_s = PubSubConverter<char*>::convert(colin_arr);
  //   (void)colin_s;

  //   using MyTup = std::tuple<double, unsigned, int, std::tuple<double, char*>>;
  //   MyTup tup(d, u, 3, {1.2, colin_arr});
  //   //    d = tup;
  //   PubSub_t<MyTup> ps_tup = PubSubConverter<MyTup>::convert(tup);
  //   //    d = ps_tup;
  //   (void)ps_tup;

  //   MyTup t3 = PubSubConverter<MyTup>::deconvert(ps_tup);
  //   (void)t3;

  //   using vec_tup_t = std::vector<std::tuple<int, bool>>;
  //   vec_tup_t vt = {{1, true}, {2, false}};
  //   PubSub_t<vec_tup_t> ps_vt = PubSubConverter<vec_tup_t>::convert(vt);
  //   vec_tup_t vt2 = PubSubConverter<vec_tup_t>::deconvert(ps_vt);
    
  //   using ps_Colin = PubSub_t<Colin>;
  //   Colin cc{3};
  //   ps_Colin pscc = PubSubConverter<Colin>::convert(cc);
  //   Colin cc2 = PubSubConverter<Colin>::deconvert(pscc);
  //   (void)cc2;

  //   using vec_bool_Colin = std::vector<std::tuple<bool, Colin>>;
  //   using ps_vec_Colin = PubSub_t<vec_bool_Colin>;
  //   vec_bool_Colin vtbC{{true, Colin{1}}, {false, Colin{0}}};
  //   ps_vec_Colin p = PubSubConverter<vec_bool_Colin>::convert(vtbC);
  //   vec_bool_Colin vtvC2 = PubSubConverter<vec_bool_Colin>::deconvert(p);

  //   using myVec_t = std::vector<int>;
  //   using nestedVec_t = std::vector<myVec_t>;
  //   myVec_t mv1{1, 2};
  //   myVec_t mv2{3, 4};
  //   nestedVec_t nestedv{mv1, mv2};
  //   using psNestedV_t = std::tuple<std::vector<size_t>, std::vector<int>>;
  //   psNestedV_t ps = PubSubConverter<nestedVec_t>::convert(nestedv);
  //   nestedVec_t retNestedv = PubSubConverter<nestedVec_t>::deconvert(ps);
  // }
}

#endif // SKYNET_PUBSUB_CONVERTER_HPP
