#include "gvirtus/transport/v2/Transport.h"

#include <mutex>
#include <utility>

namespace gvirtus::transport::v2 {

class RequestState {
 public:
  explicit RequestState(std::uint64_t id) { snapshot.request_id = id; }

  mutable std::mutex mutex;
  RequestSnapshot snapshot;
};

namespace {
bool Transition(const std::shared_ptr<RequestState> &state,
                RequestStatus expected, RequestStatus next,
                TransportError error, std::size_t transferred) {
  if (!state) return false;
  std::lock_guard<std::mutex> lock(state->mutex);
  if (state->snapshot.status != expected) return false;
  state->snapshot.status = next;
  state->snapshot.error = error;
  state->snapshot.transferred = transferred;
  return true;
}
}  // namespace

Request::Request(std::shared_ptr<RequestState> state) : state_(std::move(state)) {}

bool Request::valid() const { return static_cast<bool>(state_); }

RequestSnapshot Request::snapshot() const {
  if (!state_) return {};
  std::lock_guard<std::mutex> lock(state_->mutex);
  return state_->snapshot;
}

bool Request::Cancel() const {
  if (!state_) return false;
  std::lock_guard<std::mutex> lock(state_->mutex);
  if (IsTerminal(state_->snapshot.status)) return false;
  state_->snapshot.status = RequestStatus::Cancelled;
  state_->snapshot.error = TransportError::Cancelled;
  return true;
}

RequestCompletion::RequestCompletion(std::shared_ptr<RequestState> state)
    : state_(std::move(state)) {}

RequestCompletion RequestCompletion::Create(std::uint64_t request_id) {
  if (request_id == 0) return RequestCompletion(nullptr);
  return RequestCompletion(std::make_shared<RequestState>(request_id));
}

Request RequestCompletion::request() const { return Request(state_); }

bool RequestCompletion::MarkInProgress() const {
  return Transition(state_, RequestStatus::Queued, RequestStatus::InProgress,
                    TransportError::None, 0);
}

bool RequestCompletion::Complete(std::size_t transferred) const {
  return Transition(state_, RequestStatus::InProgress, RequestStatus::Completed,
                    TransportError::None, transferred);
}

bool RequestCompletion::Fail(TransportError error,
                             std::size_t transferred) const {
  if (error == TransportError::None || error == TransportError::Cancelled)
    return false;
  if (!state_) return false;
  std::lock_guard<std::mutex> lock(state_->mutex);
  if (state_->snapshot.status != RequestStatus::Queued &&
      state_->snapshot.status != RequestStatus::InProgress)
    return false;
  state_->snapshot.status = RequestStatus::Failed;
  state_->snapshot.error = error;
  state_->snapshot.transferred = transferred;
  return true;
}

bool IsTerminal(RequestStatus status) {
  return status == RequestStatus::Completed ||
         status == RequestStatus::Failed ||
         status == RequestStatus::Cancelled;
}

const char *ToString(TransportError error) {
  switch (error) {
    case TransportError::None: return "none";
    case TransportError::InvalidArgument: return "invalid argument";
    case TransportError::MessageTooLarge: return "message too large";
    case TransportError::Backpressure: return "transport backpressure";
    case TransportError::Unsupported: return "operation unsupported";
    case TransportError::Disconnected: return "transport disconnected";
    case TransportError::Cancelled: return "request cancelled";
    case TransportError::IoError: return "transport I/O error";
  }
  return "unknown transport error";
}

}  // namespace gvirtus::transport::v2
