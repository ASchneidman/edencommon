/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <folly/Synchronized.h>
#include <folly/container/F14Set.h>
#include <folly/futures/Promise.h>
#include <folly/synchronization/LifoSem.h>
#include <sys/types.h>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace facebook::eden {

class ProcessNameCache {
 public:
  /**
   * Create a cache that maintains process names until `expiry` has elapsed
   * without them being referenced or observed.
   */
  explicit ProcessNameCache(
      std::chrono::nanoseconds expiry = std::chrono::minutes{5});

  ~ProcessNameCache();

  /**
   * Records a reference to a pid. This is called by performance-critical code.
   * Refreshes the expiry on the given pid. The process name is read
   * asynchronously on a background thread.
   *
   * If possible, the caller should avoid calling add() with a series of
   * redundant pids.
   */
  void add(pid_t pid);

  /**
   * Called rarely to produce a map of all non-expired pids to their executable
   * names.
   */
  std::map<pid_t, std::string> getAllProcessNames();

  /**
   * Called occassionally to produce the command line name of the pid. If the
   * name has already been resolved this returns that name. Otherwise this will
   * return nullopt. In the future it may wait for the name to be resolved.
   */
  std::optional<std::string> getProcessName(pid_t pid);

 private:
  struct ProcessName {
    ProcessName(std::string n, std::chrono::steady_clock::duration d)
        : name{std::move(n)}, lastAccess{d} {}
    ProcessName(ProcessName&& other) noexcept
        : name{std::move(other.name)}, lastAccess{other.lastAccess.load()} {}

    ProcessName& operator=(ProcessName&& other) noexcept {
      name = std::move(other.name);
      lastAccess.store(other.lastAccess.load());
      return *this;
    }

    std::string name;
    mutable std::atomic<std::chrono::steady_clock::duration> lastAccess;
  };

  struct State {
    std::unordered_map<pid_t, ProcessName> names;

    // Allows periodic flushing of the expired names without quadratic-time
    // insertion. waterLevel grows twice as fast as names.size() can, and when
    // it exceeds names.size(), the name set is pruned.
    size_t waterLevel = 0;

    bool workerThreadShouldStop = false;
    folly::F14FastSet<pid_t> addQueue;
    std::vector<folly::Promise<std::map<pid_t, std::string>>> getQueue;
  };

  void clearExpired(std::chrono::steady_clock::duration now, State& state);
  void processActions();

  const std::chrono::nanoseconds expiry_;
  const std::chrono::steady_clock::time_point startPoint_;
  folly::Synchronized<State> state_;
  folly::LifoSem sem_;
  std::thread workerThread_;
};

namespace detail {
/**
 * The number of digits required for a decimal representation of a pid.
 */
constexpr size_t kMaxDecimalPidLength = 10;
static_assert(sizeof(pid_t) <= 4);

/**
 * A stack-allocated string with the contents /proc/<pid>/cmdline for any pid.
 */
using ProcPidCmdLine = std::array<
    char,
    6 /* /proc/ */ + kMaxDecimalPidLength + 8 /* /cmdline */ + 1 /* null */>;

/**
 * Returns the ProcPidCmdLine for a given pid. The result is always
 * null-terminated.
 */
ProcPidCmdLine getProcPidCmdLine(pid_t pid);

/**
 * Given a pid, returns its executable name or <err:###> with the appropriate
 * errno.
 *
 * readPidName only allocates if the resulting executable name does not fit in
 * std::string's small string optimization, which is relatively rare for
 * programs in /usr/bin.
 */
std::string readPidName(pid_t pid);
} // namespace detail

} // namespace facebook::eden
