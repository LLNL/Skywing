#ifndef SKYNET_ENABLE_LOGGING_HPP
#define SKYNET_ENABLE_LOGGING_HPP

#include "spdlog/spdlog.h"

// Macros to enable logging; if the logging level isn't high enough than these
// will do nothing
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  #define SKYNET_SET_LOG_LEVEL_TO_TRACE() ::spdlog::set_level(::spdlog::level::trace)
#else
  #define SKYNET_SET_LOG_LEVEL_TO_TRACE() (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
  #define SKYNET_SET_LOG_LEVEL_TO_DEBUG() ::spdlog::set_level(::spdlog::level::debug)
#else
  #define SKYNET_SET_LOG_LEVEL_TO_DEBUG() (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
  #define SKYNET_SET_LOG_LEVEL_TO_WARN() ::spdlog::set_level(::spdlog::level::warn)
#else
  #define SKYNET_SET_LOG_LEVEL_TO_WARN() (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
  #define SKYNET_SET_LOG_LEVEL_TO_ERROR() ::spdlog::set_level(::spdlog::level::error)
#else
  #define SKYNET_SET_LOG_LEVEL_TO_ERROR() (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
  #define SKYNET_SET_LOG_LEVEL_TO_CRITICAL() ::spdlog::set_level(::spdlog::level::critical)
#else
  #define SKYNET_SET_LOG_LEVEL_TO_CRITICAL() (void)0
#endif

// Automatically enable the set logging level; this is something that's too easy
// to forget to do
namespace skywing::internal::detail {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
static inline const auto dummy = [] {
  SKYNET_SET_LOG_LEVEL_TO_TRACE();
  return 0;
}();
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
static inline const auto dummy = [] {
  SKYNET_SET_LOG_LEVEL_TO_DEBUG();
  return 0;
}();
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_WARN
static inline const auto dummy = [] {
  SKYNET_SET_LOG_LEVEL_TO_WARN();
  return 0;
}();
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_ERROR
static inline const auto dummy = [] {
  SKYNET_SET_LOG_LEVEL_TO_ERROR();
  return 0;
}();
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_CRITICAL
static inline const auto dummy = [] {
  SKYNET_SET_LOG_LEVEL_TO_CRITICAL();
  return 0;
}();
#endif
} // namespace skywing::internal::detail

#endif // SKYNET_ENABLE_LOGGING_HPP
