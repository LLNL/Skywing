#ifndef SKYNET_INTERNAL_MANAGER_WAITER_CALLABLES_HPP
#define SKYNET_INTERNAL_MANAGER_WAITER_CALLABLES_HPP

// This header exists so that the Manager types returned from the header can be used
// by Job

#include "skywing_core/types.hpp"

#include <vector>

namespace skywing {
class Manager;

namespace internal {
class ReduceGroupBase;

class ManagerSubscribeIsDone {
public:
  ManagerSubscribeIsDone(Manager& manager, const std::vector<TagID>& tags) noexcept;
  bool operator()() const noexcept;

private:
  Manager* manager_;
  std::vector<TagID> tags_;
}; // class ManagerSubscribeIsDone

class ManagerReduceGroupIsCreated {
public:
  ManagerReduceGroupIsCreated(Manager& manager, const TagID& group_id) noexcept;
  bool operator()() const noexcept;

private:
  Manager* manager_;
  TagID group_id_;
}; // class ManagerReduceGroupIsCreated

class ManagerGetReduceGroup {
public:
  ManagerGetReduceGroup(Manager& manager, const TagID& group_id) noexcept;
  ReduceGroupBase& operator()() const noexcept;

private:
  Manager* manager_;
  TagID group_id_;
}; // class ManagerGetReduceGroup

class ManagerConnectionIsComplete {
public:
  ManagerConnectionIsComplete(Manager& manager, const std::string& address, std::uint16_t port) noexcept;
  bool operator()() const noexcept;

private:
  Manager* manager_;
  AddrPortPair address_;
}; // class ManagerConnectionIsComplete

class ManagerGetConnectionSuccess {
public:
  ManagerGetConnectionSuccess(Manager& manager, const std::string& address, std::uint16_t port) noexcept;
  bool operator()() const noexcept;

private:
  Manager* manager_;
  AddrPortPair address_;
}; // class ManagerGetConnectionSuccess

class ManagerIPSubscribeComplete {
public:
  ManagerIPSubscribeComplete(Manager& manager, const AddrPortPair& address, const std::vector<TagID>& tags, bool is_self_sub) noexcept;
  bool operator()() const noexcept;

private:
  Manager* manager_;
  AddrPortPair address_;
  std::vector<TagID> tags_;
  bool is_self_sub_;
};

class ManagerIPSubscribeSuccess {
public:
  ManagerIPSubscribeSuccess(Manager& manager, const AddrPortPair& address, const std::vector<TagID>& tags, bool is_self_sub) noexcept;
  bool operator()() const noexcept;

private:
  Manager* manager_;
  AddrPortPair address_;
  std::vector<TagID> tags_;
  bool is_self_sub_;
};
} // namespace internal
} // namespace skywing

#endif // SKYNET_INTERNAL_MANAGER_WAITER_CALLABLES_HPP
