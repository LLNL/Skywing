#ifndef ITERATIVE_RESILIENCE_POLICIES
#define ITERATIVE_RESILIENCE_POLICIES

namespace skywing
{
  /** @brief Resilience policy that does nothing. Not recommended but useful for prototyping.
   */
  struct TrivialResiliencePolicy
  {
    template<typename IterMethod, typename TagIter>
    void handle_dead_neighbor(const IterMethod& iter_method, const TagIter& tag_iter)
    { (void)iter_method; (void)tag_iter; }
  };
} // namespace skywing

#endif // ITERATIVE_RESILIENCE_POLICIES
