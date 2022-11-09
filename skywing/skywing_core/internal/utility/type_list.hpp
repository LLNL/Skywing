#ifndef SKYNET_INTERNAL_UTILITY_TYPE_LIST_HPP
#define SKYNET_INTERNAL_UTILITY_TYPE_LIST_HPP

namespace skywing::internal {
/** \brief Structure for representing a list of types
 */
template<typename... Ts>
struct TypeList {};

namespace detail {
// Base class for specialization
template<typename SearchFor, typename... Ts>
struct IndexOfImpl;

// No match, just have it be zero so the size of the list is returned
template<typename SearchFor>
struct IndexOfImpl<SearchFor, TypeList<>> {
  static constexpr int value = 0;
};

// Match
template<typename SearchFor, typename... Ts>
struct IndexOfImpl<SearchFor, TypeList<SearchFor, Ts...>> {
  static constexpr int value = 0;
};

// No match
template<typename SearchFor, typename First, typename... Rest>
struct IndexOfImpl<SearchFor, TypeList<First, Rest...>> {
  static constexpr int value = 1 + IndexOfImpl<SearchFor, TypeList<Rest...>>::value;
};

// Base
template<typename T>
struct SizeImpl;

// Match
template<typename... Ts>
struct SizeImpl<TypeList<Ts...>> {
  static constexpr int value = sizeof...(Ts);
};

// Base
template<int Index, typename... Ts>
struct AtImpl;

// Match
template<typename First, typename... Rest>
struct AtImpl<0, TypeList<First, Rest...>> {
  using Type = First;
};

// No match
template<int Index, typename First, typename... Rest>
struct AtImpl<Index, TypeList<First, Rest...>> {
  using Type = typename AtImpl<Index - 1, TypeList<Rest...>>::Type;
};

// Base
template<typename List1, typename List2>
struct AppendImpl;

// Append the types
template<typename... Ts1, typename... Ts2>
struct AppendImpl<TypeList<Ts1...>, TypeList<Ts2...>> {
  using Type = TypeList<Ts1..., Ts2...>;
};

// Base
template<typename List, template<typename...> typename Template>
struct ApplyToImpl;

template<typename... Ts, template<typename...> typename Template>
struct ApplyToImpl<TypeList<Ts...>, Template> {
  using Type = Template<Ts...>;
};

// Base
template<typename List, template<typename...> typename Template>
struct ApplyToEachImpl;

template<typename... Ts, template<typename...> typename Template>
struct ApplyToEachImpl<TypeList<Ts...>, Template> {
  using Type = TypeList<Template<Ts>...>;
};

// Base
template<typename Template>
struct ExtractTemplateTypeArgsImpl;

template<typename... Ts, template<typename...> typename Template>
struct ExtractTemplateTypeArgsImpl<Template<Ts...>> {
  using Type = TypeList<Ts...>;
};
} // namespace detail

/** \brief The index of a type in a TypeList, or the size of the list if it is not present
 */
template<typename SearchFor, typename List>
constexpr int index_of = detail::IndexOfImpl<SearchFor, List>::value;

/** \brief The size of a type list
 */
template<typename List>
constexpr int size = detail::SizeImpl<List>::value;

/** \brief The type at the specified index
 */
template<int Index, typename List>
using At = typename detail::AtImpl<Index, List>::Type;

/** \brief Appends two typelists together
 */
template<typename List1, typename List2>
using Append = typename detail::AppendImpl<List1, List2>::Type;

/** \brief Apply a type list to a template
 */
template<typename List, template<typename...> typename Template>
using ApplyTo = typename detail::ApplyToImpl<List, Template>::Type;

/** \brief Produce a TypeList by applying each member of a typelist
 * as the first argument to a template
 */
template<typename List, template<typename...> typename Template>
using ApplyToEach = typename detail::ApplyToEachImpl<List, Template>::Type;

/** \brief Extract template type arguments from a template
 */
template<typename Template>
using ExtractTemplateTypeArgs = typename detail::ExtractTemplateTypeArgsImpl<Template>::Type;
} // namespace skywing::internal

#endif // SKYNET_INTERNAL_UTILITY_TYPE_LIST_HPP
