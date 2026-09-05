#include "gvirtus/protocol/v2/ManagedMemory.h"

#include <cassert>

using namespace gvirtus::protocol::v2;

int main() {
  VirtualMemoryRegistry memory(0x1234);
  const auto allocation =
      memory.Register(0x10000000, 4096, 0, AllocationType::Managed);
  assert(allocation);

  ManagedMemoryCoherenceService coherence(0x1234);
  assert(coherence.Register(allocation.allocation));
  assert(coherence.Register(allocation.allocation).error ==
         CoherenceError::DuplicateAllocation);

  const auto prepared = coherence.PrepareLaunch(
      allocation.allocation.virtual_base, ManagedAccess::ReadWrite, 77);
  assert(prepared);
  assert(prepared.allocation.stage == CoherenceStage::UploadRequired);
  assert(prepared.allocation.stream == 77);
  assert(coherence.MarkSubmitted(allocation.allocation.virtual_base).error ==
         CoherenceError::InvalidTransition);
  assert(coherence.MarkFrontendWrite(allocation.allocation.virtual_base).error ==
         CoherenceError::Busy);

  assert(coherence.MarkUploadComplete(allocation.allocation.virtual_base));
  assert(coherence.MarkSubmitted(allocation.allocation.virtual_base));
  const auto completed =
      coherence.MarkCudaComplete(allocation.allocation.virtual_base);
  assert(completed);
  assert(completed.allocation.authority == MemoryAuthority::Backend);
  assert(completed.allocation.stage == CoherenceStage::DownloadRequired);
  assert(coherence.Release(allocation.allocation.virtual_base).error ==
         CoherenceError::Busy);

  const auto restored =
      coherence.MarkDownloadComplete(allocation.allocation.virtual_base);
  assert(restored);
  assert(restored.allocation.authority == MemoryAuthority::Coherent);
  assert(restored.allocation.stage == CoherenceStage::Idle);
  assert(restored.allocation.epoch == 1);
  const auto metrics = coherence.metrics();
  assert(metrics.uploaded_bytes == 4096 && metrics.uploads == 1);
  assert(metrics.downloaded_bytes == 4096 && metrics.downloads == 1);

  assert(coherence.MarkFrontendWrite(allocation.allocation.virtual_base));
  const auto write_only = coherence.PrepareLaunch(
      allocation.allocation.virtual_base, ManagedAccess::WriteOnly, 0);
  assert(write_only.allocation.stage == CoherenceStage::ReadyForSubmission);
  assert(coherence.MarkSubmitted(allocation.allocation.virtual_base));
  assert(coherence.MarkCudaComplete(allocation.allocation.virtual_base));
  assert(coherence.MarkDownloadComplete(allocation.allocation.virtual_base));

  assert(coherence.Release(allocation.allocation.virtual_base));
  assert(coherence.active_allocations() == 0);
  assert(coherence.Inspect(allocation.allocation.virtual_base).error ==
         CoherenceError::StaleAllocation);
  ManagedMemoryCoherenceService other_session(0x5678);
  assert(other_session.Inspect(allocation.allocation.virtual_base).error ==
         CoherenceError::CrossSessionAllocation);

  Allocation regular = allocation.allocation;
  regular.virtual_base += 0x10000;
  regular.type = AllocationType::Device;
  assert(coherence.Register(regular).error ==
         CoherenceError::NotManagedAllocation);
  return 0;
}
