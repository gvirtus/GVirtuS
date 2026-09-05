#include "TcpCommunicator.h"

#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cstring>

using gvirtus::communicators::TcpCommunicator;

int main() {
  int sockets[2];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

  TcpCommunicator communicator(sockets[0], "local-test-peer");

  const std::array<char, 7> inbound{'g', 'v', 'i', 'r', 't', 'u', 's'};
  assert(write(sockets[1], inbound.data(), 2) == 2);
  assert(write(sockets[1], inbound.data() + 2, inbound.size() - 2) ==
         static_cast<ssize_t>(inbound.size() - 2));

  std::array<char, inbound.size()> received{};
  assert(communicator.Read(received.data(), received.size()) == received.size());
  assert(received == inbound);

  const std::array<char, 5> outbound{'v', '1', '-', 'o', 'k'};
  assert(communicator.Write(outbound.data(), outbound.size()) == outbound.size());
  std::array<char, outbound.size()> echoed{};
  assert(read(sockets[1], echoed.data(), echoed.size()) ==
         static_cast<ssize_t>(echoed.size()));
  assert(echoed == outbound);

  communicator.Close();
  communicator.Close();
  assert(communicator.Read(received.data(), 0) == 0);
  close(sockets[1]);
  return 0;
}
