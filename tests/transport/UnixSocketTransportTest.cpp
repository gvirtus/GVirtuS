#include "gvirtus/transport/v2/UnixSocketTransport.h"

#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cassert>

using namespace gvirtus::transport::v2;

int main() {
  int sockets[2];
  assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

  UnixSocketTransportOptions options;
  options.limits = {32, 128, 4, 1};
  UnixSocketTransport transport(
      sockets[0], {"", "/tmp/gvirtus-server.sock", 11, 5},
      {"unix", "local-peer", 11, 5}, options);
  assert(transport.local_endpoint().transport == "unix");
  assert(transport.remote_endpoint().session_id == 5);
  assert(HasCapability(transport.capabilities(), Capability::Bulk));
  assert(!HasCapability(transport.capabilities(), Capability::Encrypted));
  assert(!HasCapability(transport.capabilities(), Capability::GpuMemory));

  const std::array<std::byte, 4> outbound{
      std::byte{'u'}, std::byte{'n'}, std::byte{'i'}, std::byte{'x'}};
  auto send = transport.SendControl({outbound.data(), outbound.size()});
  transport.Progress();
  assert(send.snapshot().status == RequestStatus::Completed);
  std::array<std::byte, 4> wire{};
  assert(::read(sockets[1], wire.data(), wire.size()) ==
         static_cast<ssize_t>(wire.size()));
  assert(wire == outbound);

  std::array<std::byte, 4> inbound{};
  auto receive = transport.ReceiveBulk(
      {inbound.data(), inbound.size()}, {MemoryClass::Host, 8});
  assert(::write(sockets[1], outbound.data(), 2) == 2);
  transport.Progress();
  assert(receive.snapshot().status == RequestStatus::InProgress);
  assert(::write(sockets[1], outbound.data() + 2, 2) == 2);
  transport.Progress();
  assert(receive.snapshot().status == RequestStatus::Completed);
  assert(inbound == outbound);

  auto cancelled = transport.ReceiveControl({inbound.data(), inbound.size()});
  assert(cancelled.Cancel());
  transport.Progress();
  assert(cancelled.snapshot().status == RequestStatus::Cancelled);

  const auto statistics = transport.statistics();
  assert(statistics.control_operations == 2);
  assert(statistics.bulk_operations == 1);
  assert(statistics.bytes_sent == 4);
  assert(statistics.bytes_received == 4);

  transport.Close();
  transport.Close();
  ::close(sockets[1]);

  int invalid_sockets[2];
  assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, invalid_sockets) == 0);
  bool rejected = false;
  try {
    UnixSocketTransport invalid(invalid_sockets[0], {"tcp", "bad", 1, 1},
                                {"unix", "peer", 1, 1});
  } catch (const std::invalid_argument &) {
    rejected = true;
    ::close(invalid_sockets[0]);
  }
  assert(rejected);
  ::close(invalid_sockets[1]);
  return 0;
}
