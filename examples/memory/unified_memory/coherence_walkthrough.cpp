#include "gvirtus/protocol/v2/ManagedMemory.h"

#include <iostream>

using namespace gvirtus::protocol::v2;

int main() {
  VirtualMemoryRegistry memory(1);
  const auto allocation =
      memory.Register(0x10000000, 1024, 0, AllocationType::Managed);
  ManagedMemoryCoherenceService coherence(1);
  if (!allocation || !coherence.Register(allocation.allocation)) return 1;

  auto state = coherence.PrepareLaunch(allocation.allocation.virtual_base,
                                       ManagedAccess::ReadWrite, 0);
  std::cout << "pre-launch stage=" << static_cast<int>(state.allocation.stage)
            << '\n';
  coherence.MarkUploadComplete(allocation.allocation.virtual_base);
  coherence.MarkSubmitted(allocation.allocation.virtual_base);
  coherence.MarkCudaComplete(allocation.allocation.virtual_base);
  state = coherence.MarkDownloadComplete(allocation.allocation.virtual_base);
  std::cout << "restored epoch=" << state.allocation.epoch << '\n';
  return state ? 0 : 1;
}
