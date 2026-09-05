#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "gvirtus/transport/v2/Transport.h"

namespace gvirtus::transport::v2 {

enum class SelectionError {
  None,
  NoCandidates,
  NoCompatibleTransport,
  PreferredTransportUnavailable,
};

struct TransportCandidate {
  std::string name;
  Capabilities capabilities = 0;
  TransportLimits limits;
  std::size_t priority = 0;
  bool available = true;
};

struct SelectionRequirements {
  std::size_t control_size = 0;
  std::size_t bulk_size = 0;
  std::size_t outstanding_requests = 1;
  std::size_t scatter_gather_entries = 1;
  MemoryClass memory_class = MemoryClass::Host;
  bool require_encryption = false;
  bool allow_staging = true;
  std::string preferred_transport;
  bool require_preferred_transport = false;
};

struct SelectionResult {
  std::size_t candidate_index = 0;
  SelectionError error = SelectionError::None;
  bool uses_staging = false;
  explicit operator bool() const { return error == SelectionError::None; }
};

SelectionResult SelectTransport(
    const std::vector<TransportCandidate> &candidates,
    const SelectionRequirements &requirements);

const char *ToString(SelectionError error);

}  // namespace gvirtus::transport::v2
