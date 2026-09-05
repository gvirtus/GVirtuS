#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "gvirtus/protocol/v2/VirtualObjects.h"

namespace gvirtus::protocol::v2 {

enum class AsyncRequestKind {
  Operation,
  StreamSynchronize,
  DeviceSynchronize,
};

enum class AsyncRequestState {
  Queued,
  Submitted,
  Completed,
  Failed,
  Cancelled,
};

enum class AsyncEngineError {
  None,
  InvalidRequestId,
  DuplicateRequestId,
  RequestIdOutOfOrder,
  Backpressure,
  UnknownRequest,
  NotReady,
  InvalidTransition,
  ResponseNotReady,
};

struct AsyncRequest {
  std::uint64_t request_id = 0;
  VirtualObject stream = 0;
  AsyncRequestKind kind = AsyncRequestKind::Operation;
  AsyncRequestState state = AsyncRequestState::Queued;
  std::set<std::uint64_t> dependencies;
  std::int32_t completion_code = 0;
};

struct AsyncResult {
  AsyncRequest request{};
  AsyncEngineError error = AsyncEngineError::None;
  explicit operator bool() const { return error == AsyncEngineError::None; }
};

class AsyncRequestEngine {
 public:
  explicit AsyncRequestEngine(std::size_t maximum_outstanding);

  AsyncResult Enqueue(std::uint64_t request_id, VirtualObject stream,
                      AsyncRequestKind kind = AsyncRequestKind::Operation);
  std::vector<AsyncRequest> Ready() const;
  AsyncResult MarkSubmitted(std::uint64_t request_id);
  AsyncResult MarkCompleted(std::uint64_t request_id,
                            std::int32_t completion_code = 0);
  AsyncResult MarkFailed(std::uint64_t request_id,
                         std::int32_t completion_code);
  AsyncResult Cancel(std::uint64_t request_id);
  std::vector<AsyncRequest> CancelAll();
  AsyncResult TakeResponse(std::uint64_t request_id);

  std::size_t outstanding() const;

 private:
  static bool IsTerminal(AsyncRequestState state);
  void CompleteDependenciesLocked(std::uint64_t request_id);
  void CancelDependentsLocked(std::uint64_t request_id);
  void ClearTailLocked(const AsyncRequest &request);

  const std::size_t maximum_outstanding_;
  mutable std::mutex mutex_;
  std::map<std::uint64_t, AsyncRequest> requests_;
  std::map<VirtualObject, std::uint64_t> stream_tails_;
  std::uint64_t device_barrier_ = 0;
  std::uint64_t highest_request_id_ = 0;
};

const char *ToString(AsyncEngineError error);

}  // namespace gvirtus::protocol::v2
