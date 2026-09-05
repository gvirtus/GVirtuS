#include "gvirtus/protocol/v2/VirtualMemory.h"

#include <limits>

namespace gvirtus::protocol::v2 {
namespace {

bool Contains(VirtualAddress base, std::uint64_t size,
              VirtualAddress address) {
  return address >= base && address - base < size;
}

}  // namespace

VirtualMemoryRegistry::VirtualMemoryRegistry(std::uint16_t session_namespace,
                                             std::size_t max_allocations)
    : session_namespace_(session_namespace),
      max_allocations_(max_allocations) {}

AllocationResult VirtualMemoryRegistry::Register(BackendAddress backend_base,
                                                 std::uint64_t size,
                                                 std::uint32_t device,
                                                 AllocationType type) {
  if (session_namespace_ == 0)
    return {{}, MemoryError::InvalidSessionNamespace};
  if (size == 0) return {{}, MemoryError::InvalidSize};
  if (backend_base == 0) return {{}, MemoryError::InvalidBackendAddress};

  std::lock_guard<std::mutex> lock(mutex_);
  if (active_.size() >= max_allocations_)
    return {{}, MemoryError::CapacityExceeded};

  if (size > kOffsetMask - next_offset_)
    return {{}, MemoryError::AddressSpaceExhausted};
  const auto end_offset = next_offset_ + size;
  if (end_offset > kOffsetMask - (kAlignment - 1))
    return {{}, MemoryError::AddressSpaceExhausted};
  const auto next = (end_offset + kAlignment - 1) & ~(kAlignment - 1);

  Allocation allocation;
  allocation.virtual_base =
      (static_cast<std::uint64_t>(session_namespace_) << 48U) | next_offset_;
  allocation.backend_base = backend_base;
  allocation.size = size;
  allocation.device = device;
  allocation.type = type;
  active_.emplace(allocation.virtual_base, allocation);
  next_offset_ = next;
  return {allocation, MemoryError::None};
}

TranslationResult VirtualMemoryRegistry::Resolve(VirtualAddress address,
                                                  std::uint64_t length) const {
  if (session_namespace_ == 0)
    return {{}, {}, 0, MemoryError::InvalidSessionNamespace};
  if (static_cast<std::uint16_t>(address >> 48U) != session_namespace_)
    return {{}, {}, 0, MemoryError::CrossSessionPointer};

  std::lock_guard<std::mutex> lock(mutex_);
  const auto upper = active_.upper_bound(address);
  if (upper == active_.begin())
    return {{}, {}, 0, ClassifyMissingPointer(address)};
  const auto candidate = std::prev(upper);
  const auto &allocation = candidate->second;
  if (!Contains(allocation.virtual_base, allocation.size, address))
    return {{}, {}, 0, ClassifyMissingPointer(address)};

  const auto offset = address - allocation.virtual_base;
  if (length > allocation.size - offset)
    return {{}, allocation, offset, MemoryError::OutOfBounds};
  if (allocation.backend_base >
      std::numeric_limits<BackendAddress>::max() - offset)
    return {{}, allocation, offset, MemoryError::OutOfBounds};
  return {allocation.backend_base + offset, allocation, offset,
          MemoryError::None};
}

AllocationResult VirtualMemoryRegistry::Release(VirtualAddress virtual_base) {
  if (session_namespace_ == 0)
    return {{}, MemoryError::InvalidSessionNamespace};
  if (static_cast<std::uint16_t>(virtual_base >> 48U) != session_namespace_)
    return {{}, MemoryError::CrossSessionPointer};

  std::lock_guard<std::mutex> lock(mutex_);
  const auto exact = active_.find(virtual_base);
  if (exact != active_.end()) {
    const auto allocation = exact->second;
    retired_.emplace(allocation.virtual_base, allocation.size);
    active_.erase(exact);
    return {allocation, MemoryError::None};
  }

  const auto upper = active_.upper_bound(virtual_base);
  if (upper != active_.begin()) {
    const auto candidate = std::prev(upper);
    if (Contains(candidate->second.virtual_base, candidate->second.size,
                 virtual_base))
      return {{}, MemoryError::InteriorPointerFree};
  }
  return {{}, ClassifyMissingPointer(virtual_base)};
}

std::size_t VirtualMemoryRegistry::active_allocations() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_.size();
}

MemoryError VirtualMemoryRegistry::ClassifyMissingPointer(
    VirtualAddress address) const {
  const auto upper = retired_.upper_bound(address);
  if (upper != retired_.begin()) {
    const auto candidate = std::prev(upper);
    if (Contains(candidate->first, candidate->second, address))
      return MemoryError::StalePointer;
  }
  return MemoryError::UnknownPointer;
}

const char *ToString(MemoryError error) {
  switch (error) {
    case MemoryError::None:
      return "none";
    case MemoryError::InvalidSessionNamespace:
      return "session namespace must be non-zero";
    case MemoryError::InvalidSize:
      return "allocation size must be non-zero";
    case MemoryError::InvalidBackendAddress:
      return "backend address must be non-zero";
    case MemoryError::CapacityExceeded:
      return "session allocation limit exceeded";
    case MemoryError::AddressSpaceExhausted:
      return "virtual address space exhausted";
    case MemoryError::CrossSessionPointer:
      return "pointer belongs to another session";
    case MemoryError::UnknownPointer:
      return "unknown virtual pointer";
    case MemoryError::StalePointer:
      return "virtual pointer refers to a released allocation";
    case MemoryError::OutOfBounds:
      return "virtual pointer range is out of bounds";
    case MemoryError::InteriorPointerFree:
      return "cannot release an interior pointer";
  }
  return "unknown virtual memory error";
}

}  // namespace gvirtus::protocol::v2
