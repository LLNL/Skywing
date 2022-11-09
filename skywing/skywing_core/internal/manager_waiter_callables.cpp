#include "skywing_core/internal/manager_waiter_callables.hpp"

#include "skywing_core/manager.hpp"

namespace skywing::internal {
ManagerSubscribeIsDone::ManagerSubscribeIsDone(Manager& manager, const std::vector<TagID>& tags) noexcept
  : manager_{&manager}, tags_{tags}
{}

bool ManagerSubscribeIsDone::operator()() const noexcept
{
  return Manager::WaiterAccessor::subscribe_is_done(*manager_, tags_);
}

ManagerReduceGroupIsCreated::ManagerReduceGroupIsCreated(Manager& manager, const TagID& group_id) noexcept
  : manager_{&manager}, group_id_{group_id}
{}

bool ManagerReduceGroupIsCreated::operator()() const noexcept
{
  return Manager::WaiterAccessor::reduce_group_is_created(*manager_, group_id_);
}

ManagerGetReduceGroup::ManagerGetReduceGroup(Manager& manager, const TagID& group_id) noexcept
  : manager_{&manager}, group_id_{group_id}
{}

ReduceGroupBase& ManagerGetReduceGroup::operator()() const noexcept
{
  return Manager::WaiterAccessor::get_reduce_group(*manager_, group_id_);
}

ManagerConnectionIsComplete::ManagerConnectionIsComplete(
  Manager& manager, const std::string& address, std::uint16_t port) noexcept
  : manager_{&manager}, address_{address, port}
{}

bool ManagerConnectionIsComplete::operator()() const noexcept
{
  return Manager::WaiterAccessor::conn_is_complete(*manager_, address_);
}

ManagerGetConnectionSuccess::ManagerGetConnectionSuccess(
  Manager& manager, const std::string& address, std::uint16_t port) noexcept
  : manager_{&manager}, address_{address, port}
{}

bool ManagerGetConnectionSuccess::operator()() const noexcept
{
  return Manager::WaiterAccessor::conn_get_success(*manager_, address_);
}

ManagerIPSubscribeComplete::ManagerIPSubscribeComplete(
  Manager& manager, const AddrPortPair& address, const std::vector<TagID>& tags, bool is_self_sub) noexcept
  : manager_{&manager}, address_{address}, tags_{tags}, is_self_sub_{is_self_sub}
{}

bool ManagerIPSubscribeComplete::operator()() const noexcept
{
  if (is_self_sub_) { return true; }
  // Wait first to see if the connection has finished processing
  return Manager::WaiterAccessor::conn_is_complete(*manager_, address_);
}

ManagerIPSubscribeSuccess::ManagerIPSubscribeSuccess(
  Manager& manager, const AddrPortPair& address, const std::vector<TagID>& tags, bool is_self_sub) noexcept
  : manager_{&manager}, address_{address}, tags_{tags}, is_self_sub_{is_self_sub}
{}

bool ManagerIPSubscribeSuccess::operator()() const noexcept
{
  return is_self_sub_
      || (Manager::WaiterAccessor::conn_get_success(*manager_, address_)
      && Manager::WaiterAccessor::subscribe_is_done(*manager_, tags_));
}

} // namespace skywing::internal
