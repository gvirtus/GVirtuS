#pragma once

#include <memory>

#include "gvirtus/transport/v2/TcpTransport.h"

namespace gvirtus::transport::v2 {

struct UnixSocketTransportOptions {
  TransportLimits limits{64 * 1024, 256 * 1024 * 1024, 256, 16};
  int send_buffer_size = 0;
  int receive_buffer_size = 0;
};

class UnixSocketTransport final : public Transport {
 public:
  UnixSocketTransport(int socket_fd, EndpointInfo local, EndpointInfo remote,
                      UnixSocketTransportOptions options = {});
  ~UnixSocketTransport() override = default;

  UnixSocketTransport(const UnixSocketTransport &) = delete;
  UnixSocketTransport &operator=(const UnixSocketTransport &) = delete;

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
  static EndpointInfo NormalizeEndpoint(EndpointInfo endpoint);
  std::unique_ptr<TcpTransport> stream_;
};

}  // namespace gvirtus::transport::v2
