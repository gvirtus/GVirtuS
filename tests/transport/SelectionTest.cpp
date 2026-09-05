#include "gvirtus/transport/v2/Selection.h"

#include <cassert>

using namespace gvirtus::transport::v2;

int main() {
  const TransportLimits tcp_limits{64, 1024, 8, 1};
  const TransportLimits fast_limits{256, 4096, 32, 8};
  const std::vector<TransportCandidate> candidates{
      {"tcp", Capability::Control | Capability::Bulk, tcp_limits, 10, true},
      {"tls", Capability::Control | Capability::Bulk | Capability::Encrypted,
       tcp_limits, 10, true},
      {"ucx", Capability::Control | Capability::Bulk |
                  Capability::ScatterGather |
                  Capability::RegisteredHostMemory,
       fast_limits, 20, true},
      {"rdma", Capability::Control | Capability::Bulk |
                   Capability::ScatterGather |
                   Capability::RegisteredHostMemory | Capability::GpuMemory,
       fast_limits, 30, false}};

  SelectionRequirements basic;
  basic.control_size = 32;
  basic.bulk_size = 512;
  auto selected = SelectTransport(candidates, basic);
  assert(selected && candidates[selected.candidate_index].name == "ucx");
  assert(!selected.uses_staging);

  basic.preferred_transport = "tcp";
  selected = SelectTransport(candidates, basic);
  assert(selected && candidates[selected.candidate_index].name == "ucx");
  basic.require_preferred_transport = true;
  selected = SelectTransport(candidates, basic);
  assert(selected && candidates[selected.candidate_index].name == "tcp");

  SelectionRequirements secure = basic;
  secure.preferred_transport.clear();
  secure.require_preferred_transport = false;
  secure.require_encryption = true;
  selected = SelectTransport(candidates, secure);
  assert(selected && candidates[selected.candidate_index].name == "tls");

  SelectionRequirements device;
  device.memory_class = MemoryClass::Device;
  selected = SelectTransport(candidates, device);
  assert(selected && candidates[selected.candidate_index].name == "ucx");
  assert(selected.uses_staging);
  device.allow_staging = false;
  assert(SelectTransport(candidates, device).error ==
         SelectionError::NoCompatibleTransport);

  SelectionRequirements unavailable;
  unavailable.preferred_transport = "rdma";
  unavailable.require_preferred_transport = true;
  assert(SelectTransport(candidates, unavailable).error ==
         SelectionError::PreferredTransportUnavailable);

  SelectionRequirements excessive;
  excessive.bulk_size = 8192;
  assert(SelectTransport(candidates, excessive).error ==
         SelectionError::NoCompatibleTransport);
  assert(SelectTransport({}, {}).error == SelectionError::NoCandidates);
  return 0;
}
