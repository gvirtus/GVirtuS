#include "gvirtus/transport/v2/Transport.h"

#include <cassert>
#include <thread>

using namespace gvirtus::transport::v2;

int main() {
  const Capabilities capabilities =
      Capability::Control | Capability::Bulk | Capability::ScatterGather;
  assert(HasCapability(capabilities, Capability::Control));
  assert(HasCapability(capabilities, Capability::Bulk));
  assert(!HasCapability(capabilities, Capability::GpuMemory));

  const auto completion = RequestCompletion::Create(42);
  const auto request = completion.request();
  assert(request.valid());
  assert(request.snapshot().request_id == 42);
  assert(request.snapshot().status == RequestStatus::Queued);
  assert(completion.MarkInProgress());
  assert(!completion.MarkInProgress());
  assert(completion.Complete(4096));
  const auto completed = request.snapshot();
  assert(completed.status == RequestStatus::Completed);
  assert(completed.transferred == 4096);
  assert(completed.error == TransportError::None);
  assert(!request.Cancel());
  assert(!completion.Fail(TransportError::IoError));

  const auto failed_completion = RequestCompletion::Create(43);
  const auto failed_request = failed_completion.request();
  assert(failed_completion.MarkInProgress());
  assert(failed_completion.Fail(TransportError::Disconnected, 12));
  assert(failed_request.snapshot().status == RequestStatus::Failed);
  assert(failed_request.snapshot().transferred == 12);

  const auto rejected_completion = RequestCompletion::Create(45);
  assert(rejected_completion.Fail(TransportError::MessageTooLarge));
  assert(rejected_completion.request().snapshot().status ==
         RequestStatus::Failed);

  const auto cancelled_completion = RequestCompletion::Create(44);
  const auto cancelled_request = cancelled_completion.request();
  assert(cancelled_request.Cancel());
  assert(cancelled_request.snapshot().error == TransportError::Cancelled);
  assert(!cancelled_completion.MarkInProgress());

  const auto invalid = RequestCompletion::Create(0).request();
  assert(!invalid.valid());

  for (std::uint64_t id = 100; id < 200; ++id) {
    const auto raced_completion = RequestCompletion::Create(id);
    const auto raced_request = raced_completion.request();
    assert(raced_completion.MarkInProgress());
    std::thread cancel([&raced_request] { raced_request.Cancel(); });
    std::thread complete([&raced_completion] {
      raced_completion.Complete(1);
    });
    cancel.join();
    complete.join();
    const auto raced = raced_request.snapshot();
    assert(IsTerminal(raced.status));
    assert(raced.status == RequestStatus::Cancelled ||
           raced.status == RequestStatus::Completed);
  }
  return 0;
}
