#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>

#include "gvirtus/protocol/v2/VirtualMemory.h"
#include "gvirtus/protocol/v2/VirtualObjects.h"

namespace gvirtus::protocol::v2 {

enum class MemoryAuthority {
  Frontend,
  Backend,
  Coherent,
};

enum class ManagedAccess {
  ReadOnly,
  WriteOnly,
  ReadWrite,
};

enum class CoherenceStage {
  Idle,
  UploadRequired,
  ReadyForSubmission,
  Submitted,
  DownloadRequired,
};

enum class CoherenceError {
  None,
  InvalidSessionNamespace,
  InvalidAllocation,
  NotManagedAllocation,
  DuplicateAllocation,
  CrossSessionAllocation,
  UnknownAllocation,
  StaleAllocation,
  InvalidTransition,
  Busy,
};

struct ManagedAllocationState {
  VirtualAddress virtual_base = 0;
  std::uint64_t size = 0;
  MemoryAuthority authority = MemoryAuthority::Frontend;
  CoherenceStage stage = CoherenceStage::Idle;
  VirtualObject stream = 0;
  std::uint64_t epoch = 0;
  bool backend_writes = false;
};

struct CoherenceMetrics {
  std::uint64_t uploaded_bytes = 0;
  std::uint64_t downloaded_bytes = 0;
  std::uint64_t uploads = 0;
  std::uint64_t downloads = 0;
};

struct CoherenceResult {
  ManagedAllocationState allocation{};
  CoherenceError error = CoherenceError::None;

  explicit operator bool() const { return error == CoherenceError::None; }
};

class ManagedMemoryCoherenceService {
 public:
  explicit ManagedMemoryCoherenceService(std::uint16_t session_namespace);

  CoherenceResult Register(const Allocation &allocation);
  CoherenceResult MarkFrontendWrite(VirtualAddress virtual_base);
  CoherenceResult PrepareLaunch(VirtualAddress virtual_base,
                                ManagedAccess access, VirtualObject stream);
  CoherenceResult MarkUploadComplete(VirtualAddress virtual_base);
  CoherenceResult MarkSubmitted(VirtualAddress virtual_base);
  CoherenceResult MarkCudaComplete(VirtualAddress virtual_base);
  CoherenceResult MarkDownloadComplete(VirtualAddress virtual_base);
  CoherenceResult Release(VirtualAddress virtual_base);
  CoherenceResult Inspect(VirtualAddress virtual_base) const;

  CoherenceMetrics metrics() const;
  std::size_t active_allocations() const;

 private:
  CoherenceResult FindLocked(VirtualAddress virtual_base) const;

  const std::uint16_t session_namespace_;
  mutable std::mutex mutex_;
  std::map<VirtualAddress, ManagedAllocationState> allocations_;
  std::map<VirtualAddress, std::uint64_t> retired_;
  CoherenceMetrics metrics_{};
};

const char *ToString(CoherenceError error);

}  // namespace gvirtus::protocol::v2
