#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>

namespace gvirtus::protocol::v2 {

using VirtualAddress = std::uint64_t;
using BackendAddress = std::uint64_t;

enum class AllocationType : std::uint8_t {
  Device,
  Managed,
  Pitched,
};

enum class MemoryError {
  None,
  InvalidSessionNamespace,
  InvalidSize,
  InvalidBackendAddress,
  CapacityExceeded,
  AddressSpaceExhausted,
  CrossSessionPointer,
  UnknownPointer,
  StalePointer,
  OutOfBounds,
  InteriorPointerFree,
};

struct Allocation {
  VirtualAddress virtual_base = 0;
  BackendAddress backend_base = 0;
  std::uint64_t size = 0;
  std::uint32_t device = 0;
  AllocationType type = AllocationType::Device;
};

struct AllocationResult {
  Allocation allocation{};
  MemoryError error = MemoryError::None;

  explicit operator bool() const { return error == MemoryError::None; }
};

struct TranslationResult {
  BackendAddress backend_address = 0;
  Allocation allocation{};
  std::uint64_t offset = 0;
  MemoryError error = MemoryError::None;

  explicit operator bool() const { return error == MemoryError::None; }
};

class VirtualMemoryRegistry {
 public:
  static constexpr std::size_t kDefaultMaxAllocations = 65536;

  explicit VirtualMemoryRegistry(
      std::uint16_t session_namespace,
      std::size_t max_allocations = kDefaultMaxAllocations);

  AllocationResult Register(BackendAddress backend_base, std::uint64_t size,
                            std::uint32_t device = 0,
                            AllocationType type = AllocationType::Device);
  TranslationResult Resolve(VirtualAddress address,
                            std::uint64_t length = 1) const;
  AllocationResult Release(VirtualAddress virtual_base);

  std::size_t active_allocations() const;
  std::uint16_t session_namespace() const { return session_namespace_; }

 private:
  static constexpr std::uint64_t kOffsetMask = 0x0000ffffffffffffULL;
  static constexpr std::uint64_t kFirstOffset = 0x0000000000010000ULL;
  static constexpr std::uint64_t kAlignment = 256;

  MemoryError ClassifyMissingPointer(VirtualAddress address) const;

  const std::uint16_t session_namespace_;
  const std::size_t max_allocations_;
  std::uint64_t next_offset_ = kFirstOffset;
  mutable std::mutex mutex_;
  std::map<VirtualAddress, Allocation> active_;
  std::map<VirtualAddress, std::uint64_t> retired_;
};

const char *ToString(MemoryError error);

}  // namespace gvirtus::protocol::v2
