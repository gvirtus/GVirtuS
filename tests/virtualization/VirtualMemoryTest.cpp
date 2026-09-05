#include "gvirtus/protocol/v2/VirtualMemory.h"

#include <cassert>
#include <cstdint>

using namespace gvirtus::protocol::v2;

int main() {
  VirtualMemoryRegistry registry(0x1234, 2);
  const auto first =
      registry.Register(0x0000000100000000ULL, 1024, 2,
                        AllocationType::Device);
  assert(first);
  assert(first.allocation.virtual_base != first.allocation.backend_base);
  assert(first.allocation.virtual_base >> 48U == 0x1234);
  assert(registry.active_allocations() == 1);

  const auto translated = registry.Resolve(first.allocation.virtual_base + 128,
                                            256);
  assert(translated);
  assert(translated.offset == 128);
  assert(translated.backend_address == 0x0000000100000080ULL);
  assert(translated.allocation.device == 2);

  assert(registry.Resolve(first.allocation.virtual_base + 1000, 25).error ==
         MemoryError::OutOfBounds);
  assert(registry.Resolve(first.allocation.virtual_base + 1024).error ==
         MemoryError::UnknownPointer);
  assert(registry.Release(first.allocation.virtual_base + 1).error ==
         MemoryError::InteriorPointerFree);

  VirtualMemoryRegistry other_session(0x5678);
  assert(other_session.Resolve(first.allocation.virtual_base).error ==
         MemoryError::CrossSessionPointer);
  assert(other_session.Release(first.allocation.virtual_base).error ==
         MemoryError::CrossSessionPointer);

  const auto released = registry.Release(first.allocation.virtual_base);
  assert(released);
  assert(released.allocation.backend_base == first.allocation.backend_base);
  assert(registry.active_allocations() == 0);
  assert(registry.Resolve(first.allocation.virtual_base + 10).error ==
         MemoryError::StalePointer);
  assert(registry.Release(first.allocation.virtual_base).error ==
         MemoryError::StalePointer);

  assert(!registry.Register(0, 32));
  assert(!registry.Register(0x1000, 0));
  assert(registry.Register(0x2000, 32));
  assert(registry.Register(0x3000, 32));
  assert(registry.Register(0x4000, 32).error == MemoryError::CapacityExceeded);

  VirtualMemoryRegistry invalid_namespace(0);
  assert(invalid_namespace.Register(0x1000, 1).error ==
         MemoryError::InvalidSessionNamespace);
  assert(invalid_namespace.Resolve(0x1000).error ==
         MemoryError::InvalidSessionNamespace);
  assert(invalid_namespace.Release(0x1000).error ==
         MemoryError::InvalidSessionNamespace);
  return 0;
}
