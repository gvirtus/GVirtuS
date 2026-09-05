#include "gvirtus/transport/v2/TcpTransport.h"

#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cstddef>

using namespace gvirtus::transport::v2;

namespace {
EndpointInfo Endpoint(const char *address, std::uint64_t connection_id) {
  return {"tcp", address, connection_id, 7};
}
}  // namespace

int main() {
  int sockets[2];
  assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

  TcpTransportOptions options;
  options.limits = {8, 16, 2, 1};
  TcpTransport transport(sockets[0], Endpoint("local", 1),
                         Endpoint("peer", 1), options);
  assert(HasCapability(transport.capabilities(), Capability::Control));
  assert(!HasCapability(transport.capabilities(), Capability::Encrypted));
  assert(transport.limits().maximum_outstanding_requests == 2);
  assert(transport.remote_endpoint().session_id == 7);

  const std::array<std::byte, 3> first{
      std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};
  const std::array<std::byte, 2> second{std::byte{'d'}, std::byte{'e'}};
  const auto first_send = transport.SendControl({first.data(), first.size()});
  const auto second_send = transport.SendControl({second.data(), second.size()});
  const auto backpressure = transport.SendControl({second.data(), second.size()});
  assert(backpressure.snapshot().error == TransportError::Backpressure);
  transport.Progress();
  transport.Progress();
  assert(first_send.snapshot().status == RequestStatus::Completed);
  assert(second_send.snapshot().status == RequestStatus::Completed);
  std::array<std::byte, 5> wire{};
  assert(::read(sockets[1], wire.data(), wire.size()) ==
         static_cast<ssize_t>(wire.size()));
  assert(wire[0] == std::byte{'a'} && wire[4] == std::byte{'e'});

  std::array<std::byte, 5> incoming{};
  const auto receive =
      transport.ReceiveControl({incoming.data(), incoming.size()});
  assert(::write(sockets[1], first.data(), first.size()) ==
         static_cast<ssize_t>(first.size()));
  transport.Progress();
  assert(receive.snapshot().status == RequestStatus::InProgress);
  assert(::write(sockets[1], second.data(), second.size()) ==
         static_cast<ssize_t>(second.size()));
  transport.Progress();
  assert(receive.snapshot().status == RequestStatus::Completed);
  assert(incoming[0] == std::byte{'a'} && incoming[4] == std::byte{'e'});

  std::array<std::byte, 17> oversized{};
  assert(transport.SendBulk({oversized.data(), oversized.size()}, {MemoryClass::Host, 1})
             .snapshot().error == TransportError::MessageTooLarge);
  assert(transport.SendBulk({first.data(), first.size()}, {})
             .snapshot().error == TransportError::InvalidArgument);
  assert(transport.SendBulk({first.data(), first.size()}, {MemoryClass::Device, 2})
             .snapshot().error == TransportError::Unsupported);

  const auto cancelled =
      transport.ReceiveControl({incoming.data(), incoming.size()});
  assert(cancelled.Cancel());
  transport.Progress();
  assert(cancelled.snapshot().status == RequestStatus::Cancelled);

  const auto disconnected =
      transport.ReceiveControl({incoming.data(), incoming.size()});
  ::close(sockets[1]);
  transport.Progress();
  assert(disconnected.snapshot().status == RequestStatus::Failed);
  assert(disconnected.snapshot().error == TransportError::Disconnected);

  const auto statistics = transport.statistics();
  assert(statistics.control_operations == 5);
  assert(statistics.bytes_sent == 5);
  assert(statistics.bytes_received == 5);
  assert(statistics.failures >= 5);
  transport.Close();
  transport.Close();
  return 0;
}
