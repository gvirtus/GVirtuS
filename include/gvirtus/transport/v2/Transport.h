#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace gvirtus::transport::v2 {

enum class Capability : std::uint64_t {
  None = 0,
  Control = 1ULL << 0,
  Bulk = 1ULL << 1,
  ScatterGather = 1ULL << 2,
  RegisteredHostMemory = 1ULL << 3,
  GpuMemory = 1ULL << 4,
  Encrypted = 1ULL << 5,
};

using Capabilities = std::uint64_t;

constexpr Capabilities operator|(Capability left, Capability right) {
  return static_cast<Capabilities>(left) | static_cast<Capabilities>(right);
}

constexpr Capabilities operator|(Capabilities left, Capability right) {
  return left | static_cast<Capabilities>(right);
}

constexpr bool HasCapability(Capabilities capabilities, Capability capability) {
  return (capabilities & static_cast<Capabilities>(capability)) != 0;
}

enum class MemoryClass {
  Host,
  PinnedHost,
  Device,
};

enum class RequestStatus {
  Queued,
  InProgress,
  Completed,
  Failed,
  Cancelled,
};

enum class TransportError {
  None,
  InvalidArgument,
  MessageTooLarge,
  Backpressure,
  Unsupported,
  Disconnected,
  Cancelled,
  IoError,
};

struct EndpointInfo {
  std::string transport;
  std::string address;
  std::uint64_t connection_id = 0;
  std::uint64_t session_id = 0;
};

struct ConstBufferView {
  const std::byte *data = nullptr;
  std::size_t size = 0;
};

struct MutableBufferView {
  std::byte *data = nullptr;
  std::size_t size = 0;
};

struct TransferOptions {
  MemoryClass memory_class = MemoryClass::Host;
  std::uint64_t transfer_id = 0;
  bool allow_staging = true;
};

struct TransportLimits {
  std::size_t maximum_control_size = 0;
  std::size_t maximum_bulk_size = 0;
  std::size_t maximum_outstanding_requests = 0;
  std::size_t maximum_scatter_gather_entries = 1;
};

struct RequestSnapshot {
  std::uint64_t request_id = 0;
  RequestStatus status = RequestStatus::Queued;
  TransportError error = TransportError::None;
  std::size_t transferred = 0;
};

struct TransportStatistics {
  std::uint64_t control_operations = 0;
  std::uint64_t bulk_operations = 0;
  std::uint64_t bytes_sent = 0;
  std::uint64_t bytes_received = 0;
  std::uint64_t progress_calls = 0;
  std::uint64_t stalls = 0;
  std::uint64_t failures = 0;
};

class RequestState;

class Request {
 public:
  Request() = default;

  bool valid() const;
  RequestSnapshot snapshot() const;
  bool Cancel() const;

 private:
  explicit Request(std::shared_ptr<RequestState> state);
  std::shared_ptr<RequestState> state_;
  friend class RequestCompletion;
};

class RequestCompletion {
 public:
  static RequestCompletion Create(std::uint64_t request_id);

  Request request() const;
  bool MarkInProgress() const;
  bool Complete(std::size_t transferred) const;
  bool Fail(TransportError error, std::size_t transferred = 0) const;

 private:
  explicit RequestCompletion(std::shared_ptr<RequestState> state);
  std::shared_ptr<RequestState> state_;
};

class Transport {
 public:
  virtual ~Transport() = default;

  virtual EndpointInfo local_endpoint() const = 0;
  virtual EndpointInfo remote_endpoint() const = 0;
  virtual Capabilities capabilities() const = 0;
  virtual TransportLimits limits() const = 0;

  virtual Request SendControl(ConstBufferView buffer) = 0;
  virtual Request ReceiveControl(MutableBufferView buffer) = 0;
  virtual Request SendBulk(ConstBufferView buffer,
                           TransferOptions options) = 0;
  virtual Request ReceiveBulk(MutableBufferView buffer,
                              TransferOptions options) = 0;

  virtual void Progress() = 0;
  virtual void Flush() = 0;
  virtual void Close() = 0;
  virtual TransportStatistics statistics() const = 0;
};

bool IsTerminal(RequestStatus status);
const char *ToString(TransportError error);

}  // namespace gvirtus::transport::v2
