#ifndef SKYNET_INTERNAL_UTILITY_ALGORITHMS
#define SKYNET_INTERNAL_UTILITY_ALGORITHMS

#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace skywing::internal {
namespace detail {
/** \brief Structure for selecting priorities for an overload set
 *
 * Add as a parameter, where higher numbered tags will attempt to be called
 * first before later ones.
 */
template<std::size_t N>
struct PriorityTag : PriorityTag<N - 1> {};

template<>
struct PriorityTag<0> {};

/** \brief Merge is available - use it
 */
template<typename T>
auto merge_impl(T& lhs, T& rhs, PriorityTag<1>) noexcept -> decltype((void)lhs.merge(rhs))
{
  lhs.merge(rhs);
}

/** \brief Merge is not available, use this as a workaround.
 */
template<typename T>
void merge_impl(T& lhs, T& rhs, PriorityTag<0>) noexcept
{
  lhs.insert(rhs.begin(), rhs.end());
  rhs.clear();
}
} // namespace detail

/** \brief Concatenates many containers into a single vector
 */
template<typename... Ts>
std::vector<std::common_type_t<typename Ts::value_type...>> concatenate(const Ts&... containers) noexcept
{
  using ::std::begin;
  using ::std::cbegin;
  using ::std::cend;
  using ::std::size;
  // Allocate enough space for all the data at the start
  std::vector<std::common_type_t<typename Ts::value_type...>> to_ret((size(containers) + ...));
  // Copy all of the data
  auto copy_loc = begin(to_ret);
  (..., (copy_loc = ::std::copy(cbegin(containers), cend(containers), copy_loc)));
  return to_ret;
}

/** \brief Merge two associative containers together
 *
 * This is only needed as a workaround for when the merge method isn't supported.
 */
template<typename T>
void merge_associative_containers(T& lhs, T& rhs) noexcept
{
  return ::skywing::internal::detail::merge_impl(lhs, rhs, detail::PriorityTag<1>{});
}

/** \brief Split a string based on a character
 *
 * \param to_split The string to split
 * \param split_char The char to split on
 * \param max_count The maximum number of times to split
 */
std::vector<std::string_view>
  split(const std::string& to_split, const char split_char, const std::size_t max_count = 0) noexcept
{
  std::vector<std::string_view> to_ret;
  auto start_pos = to_split.find(split_char);
  auto last_pos = 0;
  for (auto pos = start_pos; pos != std::string::npos; pos = to_split.find(split_char, pos + 1)) {
    // Add one as there's always one string appended at the end
    // This also makes a max_count of 0
    if (to_ret.size() + 1 == max_count) { break; }
    // to_ret.emplace_back(to_split.data() + last_pos, to_split.data() + pos);
    to_ret.emplace_back(to_split.data() + last_pos, pos - last_pos);
    last_pos = pos + 1;
  }
  // to_ret.emplace_back(to_split.data() + last_pos, to_split.data() + to_split.size());
  to_ret.emplace_back(to_split.data() + last_pos, to_split.size() - last_pos);
  return to_ret;
}

/** \brief Class representing zipped iterators
 *
 * The two iterators passed must be of the same length
 */
template<typename... Iters>
class ZippedIterEqualLength {
public:
  constexpr ZippedIterEqualLength(Iters... iters) : iters_{iters...} {}

  constexpr std::tuple<Iters...> underlying_iters() const noexcept { return iters_; }

  // STL compatible iterator stuff
  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = std::tuple<typename Iters::value_type...>;
  using reference = std::tuple<typename Iters::reference...>;
  using pointer = value_type*;

  // Iterator operators
  constexpr reference operator*() noexcept
  {
    return for_each_iter([](auto& iter) noexcept -> decltype(auto) { return *iter; });
  }
  constexpr const reference& operator*() const noexcept
  {
    return for_each_iter([](const auto& iter) noexcept -> decltype(auto) { return *iter; });
  }
  constexpr ZippedIterEqualLength& operator++() noexcept
  {
    for_each_iter([](auto& iter) noexcept -> decltype(auto) { return ++iter; });
    return *this;
  }
  friend constexpr bool operator==(const ZippedIterEqualLength& lhs, const ZippedIterEqualLength& rhs) noexcept
  {
    return lhs.iters_ == rhs.iters_;
  }
  friend constexpr bool operator!=(const ZippedIterEqualLength& lhs, const ZippedIterEqualLength& rhs) noexcept
  {
    return !(lhs == rhs);
  }

private:
  template<typename Callable>
  constexpr auto for_each_iter(Callable&& c) noexcept
  {
    return for_each_iter_impl(std::forward<Callable>(c), std::index_sequence_for<Iters...>{});
  }

  template<typename Callable, std::size_t... Is>
  constexpr auto for_each_iter_impl(Callable&& c, std::index_sequence<Is...>) noexcept
  {
    // make_tuple will remove references, which we don't want
    // this is so ugly though...
    return std::tuple<decltype(wrap_void_func(c, std::get<Is>(iters_)))...>{wrap_void_func(c, std::get<Is>(iters_))...};
  }

  std::tuple<Iters...> iters_;
};

/** \brief Zips iterators together, all of which must be equal length
 *
 * Regardless of the underlying iterator types, the returned iterator is always a forward read-only iterator
 */
template<typename... Iters>
auto zip_iter_equal_len(Iters... iters) noexcept
{
  return ZippedIterEqualLength<Iters...>{iters...};
}
} // namespace skywing::internal

#endif // SKYNET_INTERNAL_UTILITY_ALGORITHMS
