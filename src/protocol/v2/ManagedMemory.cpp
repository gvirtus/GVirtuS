#include "gvirtus/protocol/v2/ManagedMemory.h"

namespace gvirtus::protocol::v2 {

ManagedMemoryCoherenceService::ManagedMemoryCoherenceService(
    std::uint16_t session_namespace)
    : session_namespace_(session_namespace) {}

CoherenceResult ManagedMemoryCoherenceService::Register(
    const Allocation &allocation) {
  if (session_namespace_ == 0)
    return {{}, CoherenceError::InvalidSessionNamespace};
  if (allocation.virtual_base == 0 || allocation.size == 0 ||
      static_cast<std::uint16_t>(allocation.virtual_base >> 48U) !=
          session_namespace_)
    return {{}, CoherenceError::InvalidAllocation};
  if (allocation.type != AllocationType::Managed)
    return {{}, CoherenceError::NotManagedAllocation};

  std::lock_guard<std::mutex> lock(mutex_);
  if (allocations_.find(allocation.virtual_base) != allocations_.end() ||
      retired_.find(allocation.virtual_base) != retired_.end())
    return {{}, CoherenceError::DuplicateAllocation};
  ManagedAllocationState state;
  state.virtual_base = allocation.virtual_base;
  state.size = allocation.size;
  allocations_.emplace(state.virtual_base, state);
  return {state, CoherenceError::None};
}

CoherenceResult ManagedMemoryCoherenceService::MarkFrontendWrite(
    VirtualAddress virtual_base) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = FindLocked(virtual_base);
  if (!found) return found;
  if (found.allocation.stage != CoherenceStage::Idle)
    return {found.allocation, CoherenceError::Busy};
  auto &state = allocations_.at(virtual_base);
  state.authority = MemoryAuthority::Frontend;
  ++state.epoch;
  return {state, CoherenceError::None};
}

CoherenceResult ManagedMemoryCoherenceService::PrepareLaunch(
    VirtualAddress virtual_base, ManagedAccess access, VirtualObject stream) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = FindLocked(virtual_base);
  if (!found) return found;
  if (found.allocation.stage != CoherenceStage::Idle)
    return {found.allocation, CoherenceError::Busy};

  auto &state = allocations_.at(virtual_base);
  state.stream = stream;
  state.backend_writes =
      access == ManagedAccess::WriteOnly || access == ManagedAccess::ReadWrite;
  const bool backend_reads =
      access == ManagedAccess::ReadOnly || access == ManagedAccess::ReadWrite;
  state.stage = backend_reads && state.authority == MemoryAuthority::Frontend
                    ? CoherenceStage::UploadRequired
                    : CoherenceStage::ReadyForSubmission;
  return {state, CoherenceError::None};
}

CoherenceResult ManagedMemoryCoherenceService::MarkUploadComplete(
    VirtualAddress virtual_base) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = FindLocked(virtual_base);
  if (!found) return found;
  if (found.allocation.stage != CoherenceStage::UploadRequired)
    return {found.allocation, CoherenceError::InvalidTransition};
  auto &state = allocations_.at(virtual_base);
  state.authority = MemoryAuthority::Coherent;
  state.stage = CoherenceStage::ReadyForSubmission;
  metrics_.uploaded_bytes += state.size;
  ++metrics_.uploads;
  return {state, CoherenceError::None};
}

CoherenceResult ManagedMemoryCoherenceService::MarkSubmitted(
    VirtualAddress virtual_base) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = FindLocked(virtual_base);
  if (!found) return found;
  if (found.allocation.stage != CoherenceStage::ReadyForSubmission)
    return {found.allocation, CoherenceError::InvalidTransition};
  auto &state = allocations_.at(virtual_base);
  state.stage = CoherenceStage::Submitted;
  return {state, CoherenceError::None};
}

CoherenceResult ManagedMemoryCoherenceService::MarkCudaComplete(
    VirtualAddress virtual_base) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = FindLocked(virtual_base);
  if (!found) return found;
  if (found.allocation.stage != CoherenceStage::Submitted)
    return {found.allocation, CoherenceError::InvalidTransition};
  auto &state = allocations_.at(virtual_base);
  if (state.backend_writes) {
    state.authority = MemoryAuthority::Backend;
    state.stage = CoherenceStage::DownloadRequired;
  } else {
    state.stage = CoherenceStage::Idle;
    state.stream = 0;
  }
  return {state, CoherenceError::None};
}

CoherenceResult ManagedMemoryCoherenceService::MarkDownloadComplete(
    VirtualAddress virtual_base) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = FindLocked(virtual_base);
  if (!found) return found;
  if (found.allocation.stage != CoherenceStage::DownloadRequired)
    return {found.allocation, CoherenceError::InvalidTransition};
  auto &state = allocations_.at(virtual_base);
  state.authority = MemoryAuthority::Coherent;
  state.stage = CoherenceStage::Idle;
  state.stream = 0;
  state.backend_writes = false;
  ++state.epoch;
  metrics_.downloaded_bytes += state.size;
  ++metrics_.downloads;
  return {state, CoherenceError::None};
}

CoherenceResult ManagedMemoryCoherenceService::Release(
    VirtualAddress virtual_base) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto found = FindLocked(virtual_base);
  if (!found) return found;
  if (found.allocation.stage != CoherenceStage::Idle)
    return {found.allocation, CoherenceError::Busy};
  retired_.emplace(virtual_base, found.allocation.size);
  allocations_.erase(virtual_base);
  return found;
}

CoherenceResult ManagedMemoryCoherenceService::Inspect(
    VirtualAddress virtual_base) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return FindLocked(virtual_base);
}

CoherenceMetrics ManagedMemoryCoherenceService::metrics() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return metrics_;
}

std::size_t ManagedMemoryCoherenceService::active_allocations() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return allocations_.size();
}

CoherenceResult ManagedMemoryCoherenceService::FindLocked(
    VirtualAddress virtual_base) const {
  if (session_namespace_ == 0)
    return {{}, CoherenceError::InvalidSessionNamespace};
  if (static_cast<std::uint16_t>(virtual_base >> 48U) !=
      session_namespace_)
    return {{}, CoherenceError::CrossSessionAllocation};
  const auto found = allocations_.find(virtual_base);
  if (found != allocations_.end())
    return {found->second, CoherenceError::None};
  if (retired_.find(virtual_base) != retired_.end())
    return {{}, CoherenceError::StaleAllocation};
  return {{}, CoherenceError::UnknownAllocation};
}

const char *ToString(CoherenceError error) {
  switch (error) {
    case CoherenceError::None: return "none";
    case CoherenceError::InvalidSessionNamespace: return "session namespace must be non-zero";
    case CoherenceError::InvalidAllocation: return "invalid managed allocation";
    case CoherenceError::NotManagedAllocation: return "allocation is not managed memory";
    case CoherenceError::DuplicateAllocation: return "managed allocation is already registered";
    case CoherenceError::CrossSessionAllocation: return "managed allocation belongs to another session";
    case CoherenceError::UnknownAllocation: return "unknown managed allocation";
    case CoherenceError::StaleAllocation: return "managed allocation has been released";
    case CoherenceError::InvalidTransition: return "invalid coherence transition";
    case CoherenceError::Busy: return "managed allocation has in-flight work";
  }
  return "unknown managed memory error";
}

}  // namespace gvirtus::protocol::v2
