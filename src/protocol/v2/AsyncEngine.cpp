#include "gvirtus/protocol/v2/AsyncEngine.h"

#include <algorithm>

namespace gvirtus::protocol::v2 {

AsyncRequestEngine::AsyncRequestEngine(std::size_t maximum_outstanding)
    : maximum_outstanding_(maximum_outstanding) {}

AsyncResult AsyncRequestEngine::Enqueue(std::uint64_t request_id,
                                        VirtualObject stream,
                                        AsyncRequestKind kind) {
  if (request_id == 0) return {{}, AsyncEngineError::InvalidRequestId};
  std::lock_guard<std::mutex> lock(mutex_);
  if (requests_.find(request_id) != requests_.end())
    return {{}, AsyncEngineError::DuplicateRequestId};
  if (request_id <= highest_request_id_)
    return {{}, AsyncEngineError::RequestIdOutOfOrder};
  if (requests_.size() >= maximum_outstanding_)
    return {{}, AsyncEngineError::Backpressure};

  AsyncRequest request;
  request.request_id = request_id;
  request.stream = stream;
  request.kind = kind;
  if (device_barrier_ != 0) request.dependencies.insert(device_barrier_);

  if (kind == AsyncRequestKind::DeviceSynchronize) {
    for (const auto &entry : requests_) {
      if (!IsTerminal(entry.second.state))
        request.dependencies.insert(entry.first);
    }
    device_barrier_ = request_id;
  } else {
    const auto tail = stream_tails_.find(stream);
    if (tail != stream_tails_.end()) request.dependencies.insert(tail->second);
    stream_tails_[stream] = request_id;
  }

  requests_.emplace(request_id, request);
  highest_request_id_ = request_id;
  return {request, AsyncEngineError::None};
}

std::vector<AsyncRequest> AsyncRequestEngine::Ready() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<AsyncRequest> ready;
  for (const auto &entry : requests_) {
    if (entry.second.state == AsyncRequestState::Queued &&
        entry.second.dependencies.empty())
      ready.push_back(entry.second);
  }
  return ready;
}

AsyncResult AsyncRequestEngine::MarkSubmitted(std::uint64_t request_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = requests_.find(request_id);
  if (found == requests_.end()) return {{}, AsyncEngineError::UnknownRequest};
  if (found->second.state != AsyncRequestState::Queued)
    return {found->second, AsyncEngineError::InvalidTransition};
  if (!found->second.dependencies.empty())
    return {found->second, AsyncEngineError::NotReady};
  found->second.state = AsyncRequestState::Submitted;
  return {found->second, AsyncEngineError::None};
}

AsyncResult AsyncRequestEngine::MarkCompleted(std::uint64_t request_id,
                                              std::int32_t completion_code) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = requests_.find(request_id);
  if (found == requests_.end()) return {{}, AsyncEngineError::UnknownRequest};
  if (found->second.state != AsyncRequestState::Submitted)
    return {found->second, AsyncEngineError::InvalidTransition};
  found->second.state = AsyncRequestState::Completed;
  found->second.completion_code = completion_code;
  CompleteDependenciesLocked(request_id);
  ClearTailLocked(found->second);
  return {found->second, AsyncEngineError::None};
}

AsyncResult AsyncRequestEngine::MarkFailed(std::uint64_t request_id,
                                           std::int32_t completion_code) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = requests_.find(request_id);
  if (found == requests_.end()) return {{}, AsyncEngineError::UnknownRequest};
  if (found->second.state != AsyncRequestState::Submitted)
    return {found->second, AsyncEngineError::InvalidTransition};
  found->second.state = AsyncRequestState::Failed;
  found->second.completion_code = completion_code;
  CancelDependentsLocked(request_id);
  ClearTailLocked(found->second);
  return {found->second, AsyncEngineError::None};
}

AsyncResult AsyncRequestEngine::Cancel(std::uint64_t request_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = requests_.find(request_id);
  if (found == requests_.end()) return {{}, AsyncEngineError::UnknownRequest};
  if (IsTerminal(found->second.state))
    return {found->second, AsyncEngineError::InvalidTransition};
  found->second.state = AsyncRequestState::Cancelled;
  CancelDependentsLocked(request_id);
  ClearTailLocked(found->second);
  return {found->second, AsyncEngineError::None};
}

std::vector<AsyncRequest> AsyncRequestEngine::CancelAll() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<AsyncRequest> cancelled;
  for (auto &entry : requests_) {
    if (!IsTerminal(entry.second.state)) {
      entry.second.state = AsyncRequestState::Cancelled;
      cancelled.push_back(entry.second);
    }
  }
  stream_tails_.clear();
  device_barrier_ = 0;
  return cancelled;
}

AsyncResult AsyncRequestEngine::TakeResponse(std::uint64_t request_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = requests_.find(request_id);
  if (found == requests_.end()) return {{}, AsyncEngineError::UnknownRequest};
  if (!IsTerminal(found->second.state))
    return {found->second, AsyncEngineError::ResponseNotReady};
  const auto request = found->second;
  requests_.erase(found);
  return {request, AsyncEngineError::None};
}

std::size_t AsyncRequestEngine::outstanding() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return requests_.size();
}

bool AsyncRequestEngine::IsTerminal(AsyncRequestState state) {
  return state == AsyncRequestState::Completed ||
         state == AsyncRequestState::Failed ||
         state == AsyncRequestState::Cancelled;
}

void AsyncRequestEngine::CompleteDependenciesLocked(std::uint64_t request_id) {
  for (auto &entry : requests_)
    entry.second.dependencies.erase(request_id);
}

void AsyncRequestEngine::CancelDependentsLocked(std::uint64_t request_id) {
  std::vector<std::uint64_t> newly_cancelled;
  for (auto &entry : requests_) {
    if (!IsTerminal(entry.second.state) &&
        entry.second.dependencies.find(request_id) !=
            entry.second.dependencies.end()) {
      entry.second.state = AsyncRequestState::Cancelled;
      newly_cancelled.push_back(entry.first);
    }
  }
  for (const auto dependent : newly_cancelled) {
    ClearTailLocked(requests_.at(dependent));
    CancelDependentsLocked(dependent);
  }
}

void AsyncRequestEngine::ClearTailLocked(const AsyncRequest &request) {
  const auto tail = stream_tails_.find(request.stream);
  if (tail != stream_tails_.end() && tail->second == request.request_id)
    stream_tails_.erase(tail);
  if (device_barrier_ == request.request_id) device_barrier_ = 0;
}

const char *ToString(AsyncEngineError error) {
  switch (error) {
    case AsyncEngineError::None: return "none";
    case AsyncEngineError::InvalidRequestId: return "request id must be non-zero";
    case AsyncEngineError::DuplicateRequestId: return "request id is already outstanding";
    case AsyncEngineError::RequestIdOutOfOrder: return "request id is not increasing";
    case AsyncEngineError::Backpressure: return "maximum outstanding requests reached";
    case AsyncEngineError::UnknownRequest: return "unknown request id";
    case AsyncEngineError::NotReady: return "request dependencies are incomplete";
    case AsyncEngineError::InvalidTransition: return "invalid request state transition";
    case AsyncEngineError::ResponseNotReady: return "request has no terminal response";
  }
  return "unknown asynchronous engine error";
}

}  // namespace gvirtus::protocol::v2
