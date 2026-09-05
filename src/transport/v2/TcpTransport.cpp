#include "gvirtus/transport/v2/TcpTransport.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace gvirtus::transport::v2 {
namespace {

void SetBooleanOption(int fd, int level, int name, bool enabled) {
  const int value = enabled ? 1 : 0;
  if (::setsockopt(fd, level, name, &value, sizeof(value)) != 0 &&
      errno != ENOPROTOOPT && errno != EOPNOTSUPP && errno != EINVAL)
    throw std::runtime_error(std::string("TCP socket option failed: ") +
                             std::strerror(errno));
}

void ConfigureSocket(int fd, const TcpTransportOptions &options) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
    throw std::runtime_error(std::string("TCP nonblocking mode failed: ") +
                             std::strerror(errno));
  SetBooleanOption(fd, SOL_SOCKET, SO_KEEPALIVE, options.keepalive);
  SetBooleanOption(fd, IPPROTO_TCP, TCP_NODELAY, options.no_delay);
#ifdef SO_NOSIGPIPE
  SetBooleanOption(fd, SOL_SOCKET, SO_NOSIGPIPE, true);
#endif
  if (options.send_buffer_size > 0 &&
      ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &options.send_buffer_size,
                   sizeof(options.send_buffer_size)) != 0)
    throw std::runtime_error(std::string("TCP send buffer setup failed: ") +
                             std::strerror(errno));
  if (options.receive_buffer_size > 0 &&
      ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &options.receive_buffer_size,
                   sizeof(options.receive_buffer_size)) != 0)
    throw std::runtime_error(std::string("TCP receive buffer setup failed: ") +
                             std::strerror(errno));
}

bool WouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }

}  // namespace

TcpTransport::TcpTransport(int socket_fd, EndpointInfo local,
                           EndpointInfo remote, TcpTransportOptions options)
    : socket_fd_(socket_fd),
      local_(std::move(local)),
      remote_(std::move(remote)),
      options_(options) {
  if (socket_fd_ < 0) throw std::invalid_argument("invalid TCP socket");
  if (options_.limits.maximum_control_size == 0 ||
      options_.limits.maximum_bulk_size == 0 ||
      options_.limits.maximum_outstanding_requests == 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
    throw std::invalid_argument("TCP transport limits must be non-zero");
  }
  try {
    ConfigureSocket(socket_fd_, options_);
  } catch (...) {
    ::close(socket_fd_);
    socket_fd_ = -1;
    throw;
  }
}

TcpTransport::~TcpTransport() { Close(); }

EndpointInfo TcpTransport::local_endpoint() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return local_;
}

EndpointInfo TcpTransport::remote_endpoint() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return remote_;
}

Capabilities TcpTransport::capabilities() const {
  return Capability::Control | Capability::Bulk;
}

TransportLimits TcpTransport::limits() const { return options_.limits; }

Request TcpTransport::SendControl(ConstBufferView buffer) {
  return QueueSend(buffer, options_.limits.maximum_control_size, true, nullptr);
}

Request TcpTransport::ReceiveControl(MutableBufferView buffer) {
  return QueueReceive(buffer, options_.limits.maximum_control_size, true,
                      nullptr);
}

Request TcpTransport::SendBulk(ConstBufferView buffer,
                               TransferOptions options) {
  return QueueSend(buffer, options_.limits.maximum_bulk_size, false, &options);
}

Request TcpTransport::ReceiveBulk(MutableBufferView buffer,
                                  TransferOptions options) {
  return QueueReceive(buffer, options_.limits.maximum_bulk_size, false,
                      &options);
}

Request TcpTransport::QueueSend(ConstBufferView buffer,
                                std::size_t maximum_size, bool control,
                                const TransferOptions *options) {
  std::lock_guard<std::mutex> lock(mutex_);
  RemoveCancelledLocked();
  if (socket_fd_ < 0) return RejectLocked(TransportError::Disconnected);
  if ((buffer.data == nullptr && buffer.size != 0) ||
      (options != nullptr && options->transfer_id == 0))
    return RejectLocked(TransportError::InvalidArgument);
  if (options != nullptr && options->memory_class == MemoryClass::Device)
    return RejectLocked(TransportError::Unsupported);
  if (buffer.size > maximum_size)
    return RejectLocked(TransportError::MessageTooLarge);
  if (!HasCapacityLocked()) return RejectLocked(TransportError::Backpressure);

  auto completion = RequestCompletion::Create(next_request_id_++);
  SendOperation operation;
  if (buffer.size != 0)
    operation.bytes.assign(buffer.data, buffer.data + buffer.size);
  operation.completion = completion;
  sends_.push_back(std::move(operation));
  if (control)
    ++statistics_.control_operations;
  else
    ++statistics_.bulk_operations;
  return completion.request();
}

Request TcpTransport::QueueReceive(MutableBufferView buffer,
                                   std::size_t maximum_size, bool control,
                                   const TransferOptions *options) {
  std::lock_guard<std::mutex> lock(mutex_);
  RemoveCancelledLocked();
  if (socket_fd_ < 0) return RejectLocked(TransportError::Disconnected);
  if ((buffer.data == nullptr && buffer.size != 0) ||
      (options != nullptr && options->transfer_id == 0))
    return RejectLocked(TransportError::InvalidArgument);
  if (options != nullptr && options->memory_class == MemoryClass::Device)
    return RejectLocked(TransportError::Unsupported);
  if (buffer.size > maximum_size)
    return RejectLocked(TransportError::MessageTooLarge);
  if (!HasCapacityLocked()) return RejectLocked(TransportError::Backpressure);

  auto completion = RequestCompletion::Create(next_request_id_++);
  receives_.push_back({buffer, 0, completion});
  if (control)
    ++statistics_.control_operations;
  else
    ++statistics_.bulk_operations;
  return completion.request();
}

Request TcpTransport::RejectLocked(TransportError error) {
  auto completion = RequestCompletion::Create(next_request_id_++);
  completion.Fail(error);
  ++statistics_.failures;
  return completion.request();
}

bool TcpTransport::HasCapacityLocked() const {
  return sends_.size() + receives_.size() <
         options_.limits.maximum_outstanding_requests;
}

void TcpTransport::RemoveCancelledLocked() {
  while (!sends_.empty() &&
         sends_.front().completion.request().snapshot().status ==
             RequestStatus::Cancelled)
    sends_.pop_front();
  while (!receives_.empty() &&
         receives_.front().completion.request().snapshot().status ==
             RequestStatus::Cancelled)
    receives_.pop_front();
}

void TcpTransport::Progress() {
  std::lock_guard<std::mutex> lock(mutex_);
  ++statistics_.progress_calls;
  RemoveCancelledLocked();
  if (socket_fd_ < 0) return;

  bool made_progress = false;
  if (!sends_.empty()) {
    auto &operation = sends_.front();
    if (operation.completion.request().snapshot().status ==
        RequestStatus::Queued)
      operation.completion.MarkInProgress();
    const auto remaining = operation.bytes.size() - operation.offset;
    if (remaining == 0) {
      operation.completion.Complete(0);
      sends_.pop_front();
      made_progress = true;
    } else {
#ifdef MSG_NOSIGNAL
      const auto result = ::send(socket_fd_, operation.bytes.data() + operation.offset,
                                 remaining, MSG_NOSIGNAL);
#else
      const auto result = ::send(socket_fd_, operation.bytes.data() + operation.offset,
                                 remaining, 0);
#endif
      if (result > 0) {
        const auto count = static_cast<std::size_t>(result);
        operation.offset += count;
        statistics_.bytes_sent += count;
        made_progress = true;
        if (operation.offset == operation.bytes.size()) {
          operation.completion.Complete(operation.offset);
          sends_.pop_front();
        }
      } else if (result == 0 || (errno != EINTR && !WouldBlock())) {
        FailAllLocked(result == 0 ? TransportError::Disconnected
                                  : TransportError::IoError);
        CloseLocked();
        return;
      }
    }
  }

  if (!receives_.empty()) {
    auto &operation = receives_.front();
    if (operation.completion.request().snapshot().status ==
        RequestStatus::Queued)
      operation.completion.MarkInProgress();
    const auto remaining = operation.buffer.size - operation.offset;
    if (remaining == 0) {
      operation.completion.Complete(0);
      receives_.pop_front();
      made_progress = true;
    } else {
      const auto result = ::recv(socket_fd_, operation.buffer.data + operation.offset,
                                 remaining, 0);
      if (result > 0) {
        const auto count = static_cast<std::size_t>(result);
        operation.offset += count;
        statistics_.bytes_received += count;
        made_progress = true;
        if (operation.offset == operation.buffer.size) {
          operation.completion.Complete(operation.offset);
          receives_.pop_front();
        }
      } else if (result == 0 || (errno != EINTR && !WouldBlock())) {
        FailAllLocked(result == 0 ? TransportError::Disconnected
                                  : TransportError::IoError);
        CloseLocked();
        return;
      }
    }
  }
  if (!made_progress && (!sends_.empty() || !receives_.empty()))
    ++statistics_.stalls;
}

void TcpTransport::Flush() {
  for (;;) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      RemoveCancelledLocked();
      if (socket_fd_ < 0 || sends_.empty()) return;
    }
    Progress();
  }
}

void TcpTransport::Close() {
  std::lock_guard<std::mutex> lock(mutex_);
  FailAllLocked(TransportError::Disconnected);
  CloseLocked();
}

void TcpTransport::CloseLocked() {
  if (socket_fd_ < 0) return;
  ::shutdown(socket_fd_, SHUT_RDWR);
  ::close(socket_fd_);
  socket_fd_ = -1;
}

void TcpTransport::FailAllLocked(TransportError error) {
  for (auto &operation : sends_)
    if (operation.completion.Fail(error, operation.offset)) ++statistics_.failures;
  for (auto &operation : receives_)
    if (operation.completion.Fail(error, operation.offset)) ++statistics_.failures;
  sends_.clear();
  receives_.clear();
}

TransportStatistics TcpTransport::statistics() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return statistics_;
}

}  // namespace gvirtus::transport::v2
