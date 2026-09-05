#include "gvirtus/transport/v2/UnixSocketTransport.h"

#include <stdexcept>
#include <utility>

namespace gvirtus::transport::v2 {

UnixSocketTransport::UnixSocketTransport(
    int socket_fd, EndpointInfo local, EndpointInfo remote,
    UnixSocketTransportOptions options) {
  TcpTransportOptions stream_options;
  stream_options.limits = options.limits;
  stream_options.send_buffer_size = options.send_buffer_size;
  stream_options.receive_buffer_size = options.receive_buffer_size;
  stream_options.keepalive = false;
  stream_options.no_delay = false;
  stream_ = std::make_unique<TcpTransport>(
      socket_fd, NormalizeEndpoint(std::move(local)),
      NormalizeEndpoint(std::move(remote)), stream_options);
}

EndpointInfo UnixSocketTransport::NormalizeEndpoint(EndpointInfo endpoint) {
  if (!endpoint.transport.empty() && endpoint.transport != "unix")
    throw std::invalid_argument("Unix endpoint has non-Unix transport name");
  endpoint.transport = "unix";
  return endpoint;
}

EndpointInfo UnixSocketTransport::local_endpoint() const {
  return stream_->local_endpoint();
}

EndpointInfo UnixSocketTransport::remote_endpoint() const {
  return stream_->remote_endpoint();
}

Capabilities UnixSocketTransport::capabilities() const {
  return Capability::Control | Capability::Bulk;
}

TransportLimits UnixSocketTransport::limits() const { return stream_->limits(); }

Request UnixSocketTransport::SendControl(ConstBufferView buffer) {
  return stream_->SendControl(buffer);
}

Request UnixSocketTransport::ReceiveControl(MutableBufferView buffer) {
  return stream_->ReceiveControl(buffer);
}

Request UnixSocketTransport::SendBulk(ConstBufferView buffer,
                                      TransferOptions options) {
  return stream_->SendBulk(buffer, options);
}

Request UnixSocketTransport::ReceiveBulk(MutableBufferView buffer,
                                         TransferOptions options) {
  return stream_->ReceiveBulk(buffer, options);
}

void UnixSocketTransport::Progress() { stream_->Progress(); }

void UnixSocketTransport::Flush() { stream_->Flush(); }

void UnixSocketTransport::Close() { stream_->Close(); }

TransportStatistics UnixSocketTransport::statistics() const {
  return stream_->statistics();
}

}  // namespace gvirtus::transport::v2
