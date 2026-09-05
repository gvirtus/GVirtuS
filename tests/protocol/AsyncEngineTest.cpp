#include "gvirtus/protocol/v2/AsyncEngine.h"

#include <algorithm>
#include <cassert>
#include <cstdint>

using namespace gvirtus::protocol::v2;

namespace {
bool Contains(const std::vector<AsyncRequest> &requests, std::uint64_t id) {
  return std::any_of(requests.begin(), requests.end(),
                     [id](const auto &request) {
                       return request.request_id == id;
                     });
}
}  // namespace

int main() {
  AsyncRequestEngine engine(4);
  assert(engine.Enqueue(1, 10));
  assert(engine.Enqueue(2, 10));
  assert(engine.Enqueue(3, 20));
  assert(engine.Enqueue(1, 20).error == AsyncEngineError::DuplicateRequestId);
  assert(engine.Enqueue(0, 20).error == AsyncEngineError::InvalidRequestId);

  auto ready = engine.Ready();
  assert(Contains(ready, 1) && Contains(ready, 3));
  assert(!Contains(ready, 2));
  assert(engine.MarkSubmitted(2).error == AsyncEngineError::NotReady);
  assert(engine.MarkSubmitted(1));
  assert(engine.TakeResponse(1).error == AsyncEngineError::ResponseNotReady);
  assert(engine.MarkCompleted(1, 7));
  assert(Contains(engine.Ready(), 2));
  const auto response = engine.TakeResponse(1);
  assert(response && response.request.completion_code == 7);
  assert(engine.Enqueue(1, 20).error ==
         AsyncEngineError::RequestIdOutOfOrder);

  assert(engine.Enqueue(4, 0, AsyncRequestKind::DeviceSynchronize));
  assert(engine.Enqueue(5, 30));
  assert(engine.Enqueue(6, 40).error == AsyncEngineError::Backpressure);
  assert(!Contains(engine.Ready(), 4));
  assert(!Contains(engine.Ready(), 5));

  assert(engine.MarkSubmitted(2));
  assert(engine.MarkCompleted(2));
  assert(engine.MarkSubmitted(3));
  assert(engine.MarkCompleted(3));
  assert(Contains(engine.Ready(), 4));
  assert(engine.MarkSubmitted(4));
  assert(engine.MarkCompleted(4));
  assert(Contains(engine.Ready(), 5));

  assert(engine.MarkSubmitted(5));
  assert(engine.MarkFailed(5, -1));
  assert(engine.TakeResponse(5).request.state == AsyncRequestState::Failed);

  AsyncRequestEngine cancellation(8);
  assert(cancellation.Enqueue(10, 1));
  assert(cancellation.Enqueue(11, 1));
  assert(cancellation.Enqueue(12, 1));
  assert(cancellation.Cancel(10));
  assert(cancellation.TakeResponse(11).request.state ==
         AsyncRequestState::Cancelled);
  assert(cancellation.TakeResponse(12).request.state ==
         AsyncRequestState::Cancelled);
  assert(cancellation.Enqueue(13, 1));
  assert(Contains(cancellation.Ready(), 13));
  assert(cancellation.CancelAll().size() == 1);
  return 0;
}
