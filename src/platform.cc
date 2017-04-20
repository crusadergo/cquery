#include "platform.h"

#include <iostream>
#include <iterator>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

namespace {

bool gModificationTimeDisabled = false;

// See http://stackoverflow.com/a/236803
template<typename Out>
void Split(const std::string &s, char delim, Out result) {
  std::stringstream ss;
  ss.str(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    if (!item.empty())
      *(result++) = item;
  }
}
std::vector<std::string> Split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  Split(s, delim, std::back_inserter(elems));
  return elems;
}

std::string Join(const std::vector<std::string>& entries, char delim, int end) {
  std::string result;
  bool first = true;
  for (int i = 0; i < end; ++i) {
    if (!first)
      result += delim;
    first = false;
    result += entries[i];
  }
  return result;
}

}  // namespace

PlatformMutex::~PlatformMutex() = default;

PlatformScopedMutexLock::~PlatformScopedMutexLock() = default;

PlatformSharedMemory::~PlatformSharedMemory() = default;

void MakeDirectoryRecursive(std::string path) {
  path = NormalizePath(path);

  if (TryMakeDirectory(path))
    return;

  std::string prefix = "";
  if (path[0] == '/')
    prefix = "/";

  std::vector<std::string> components = Split(path, '/');

  // Find first parent directory which doesn't exist.
  int first_success = -1;
  for (int i = components.size(); i > 0; --i) {
    if (TryMakeDirectory(prefix + Join(components, '/', i))) {
      first_success = i;
      break;
    }
  }

  if (first_success == -1) {
    std::cerr << "Failed to make any parent directory for " << path << std::endl;
    exit(1);
  }

  // Make all child directories.
  for (int i = first_success + 1; i <= components.size(); ++i) {
    if (TryMakeDirectory(prefix + Join(components, '/', i)) == false) {
      std::cerr << "Failed making directory for " << path << " even after creating parent directories" << std::endl;
      exit(1);
    }
  }
}

void DisableModificationTimeForTest() {
  gModificationTimeDisabled = true;
}

bool IsModificationTimeDisabledForTests() {
  return gModificationTimeDisabled;
}


TEST_SUITE("Platform");

TEST_CASE("Split strings") {
  std::vector<std::string> actual = Split("/a/b/c/", '/');
  std::vector<std::string> expected{"a", "b", "c"};
  REQUIRE(actual == expected);
}

TEST_CASE("Mutex lock/unlock (single process)") {
  auto m1 = CreatePlatformMutex("indexer-platformmutexttest");
  auto l1 = CreatePlatformScopedMutexLock(m1.get());
  auto m2 = CreatePlatformMutex("indexer-platformmutexttest");

  int value = 0;

  volatile bool did_run = false;
  std::thread t([&]() {
    did_run = true;
    auto l2 = CreatePlatformScopedMutexLock(m2.get());
    value = 1;
  });
  while (!did_run)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Other thread has had a chance to run, but it should not have
  // written to value yet (ie, it should be waiting).
  REQUIRE(value == 0);

  // Release the lock, wait for other thread to finish. Verify it
  // wrote the expected value.
  l1.reset();
  t.join();
  REQUIRE(value == 1);
}

TEST_SUITE_END();
