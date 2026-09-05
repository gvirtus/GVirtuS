#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include "gvirtus/transport/v2/Transport.h"

namespace gvirtus::transport::v2 {

struct TcpTransportOptions {
  TransportLimits limits{64 * 1024, 256 * 1024 * 1024, 256, 16};
  int send_buffer_size = 0;
  int receive_buffer_size = 0;
  bool keepalive = true;
  bool no_delay = true;
};

class TcpTransport final : public Transport {
 public:
  TcpTransport(int socket_fd, EndpointInfo local, EndpointInfo remote,
               TcpTransportOptions options = {});
  ~TcpTransport() override;

  TcpTransport(const TcpTransport &) = delete;
  TcpTransport &operator=(const TcpTransport &) = delete;

  EndpointInfo local_endpoint() const override;
  EndpointInfo remote_endpoint() const override;
  Capabilities capabilities() const override;
  TransportLimits limits() const override;

  Request SendControl(ConstBufferView buffer) override;
  Request ReceiveControl(MutableBufferView buffer) override;
  Request SendBulk(ConstBufferView buffer, TransferOptions options) override;
  Request ReceiveBulk(MutableBufferView buffer,
                      TransferOptions options) override;

  void Progress() override;
  void Flush() override;
  void Close() override;
  TransportStatistics statistics() const override;

 private:
  struct SendOperation {
    std::vector<std::byte> bytes;
    std::size_t offset = 0;
    RequestCompletion completion = RequestCompletion::Create(0);
  };

  struct ReceiveOperation {
    MutableBufferView buffer;
    std::size_t offset = 0;
    RequestCompletion completion = RequestCompletion::Create(0);
  };

  Request QueueSend(ConstBufferView buffer, std::size_t maximum_size,
                    bool control, const TransferOptions *options);
  Request QueueReceive(MutableBufferView buffer, std::size_t maximum_size,
                       bool control, const TransferOptions *options);
  Request RejectLocked(TransportError error);
  bool HasCapacityLocked() const;
  void RemoveCancelledLocked();
  void FailAllLocked(TransportError error);
  void CloseLocked();

  mutable std::mutex mutex_;
  int socket_fd_ = -1;
  EndpointInfo local_;
  EndpointInfo remote_;
  TcpTransportOptions options_;
  std::uint64_t next_request_id_ = 1;
  std::deque<SendOperation> sends_;
  std::deque<ReceiveOperation> receives_;
  TransportStatistics statistics_;
};

}  // namespace gvirtus::transport::v2
