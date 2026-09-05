#include "gvirtus/transport/v2/Selection.h"

#include <limits>
#include <tuple>

namespace gvirtus::transport::v2 {
namespace {

struct Compatibility {
  bool compatible = false;
  bool uses_staging = false;
};

Compatibility IsCompatible(const TransportCandidate &candidate,
                           const SelectionRequirements &requirements) {
  if (!candidate.available || candidate.name.empty()) return {};
  if (!HasCapability(candidate.capabilities, Capability::Control) ||
      (requirements.bulk_size != 0 &&
       !HasCapability(candidate.capabilities, Capability::Bulk)) ||
      (requirements.scatter_gather_entries > 1 &&
       !HasCapability(candidate.capabilities, Capability::ScatterGather)) ||
      (requirements.require_encryption &&
       !HasCapability(candidate.capabilities, Capability::Encrypted)))
    return {};
  if (requirements.control_size > candidate.limits.maximum_control_size ||
      requirements.bulk_size > candidate.limits.maximum_bulk_size ||
      requirements.outstanding_requests == 0 ||
      requirements.outstanding_requests >
          candidate.limits.maximum_outstanding_requests ||
      requirements.scatter_gather_entries == 0 ||
      requirements.scatter_gather_entries >
          candidate.limits.maximum_scatter_gather_entries)
    return {};

  if (requirements.memory_class == MemoryClass::Device &&
      !HasCapability(candidate.capabilities, Capability::GpuMemory)) {
    if (!requirements.allow_staging ||
        !HasCapability(candidate.capabilities,
                       Capability::RegisteredHostMemory))
      return {};
    return {true, true};
  }
  if (requirements.memory_class == MemoryClass::PinnedHost &&
      !HasCapability(candidate.capabilities, Capability::RegisteredHostMemory))
    return {};
  return {true, false};
}

std::size_t CapabilityScore(Capabilities capabilities) {
  std::size_t score = 0;
  if (HasCapability(capabilities, Capability::GpuMemory)) score += 8;
  if (HasCapability(capabilities, Capability::RegisteredHostMemory)) score += 4;
  if (HasCapability(capabilities, Capability::ScatterGather)) score += 2;
  if (HasCapability(capabilities, Capability::Encrypted)) score += 1;
  return score;
}

}  // namespace

SelectionResult SelectTransport(
    const std::vector<TransportCandidate> &candidates,
    const SelectionRequirements &requirements) {
  if (candidates.empty()) return {{}, SelectionError::NoCandidates, false};

  std::size_t selected = std::numeric_limits<std::size_t>::max();
  bool selected_staging = false;
  std::tuple<std::size_t, bool, std::size_t, std::size_t> selected_rank{};
  bool preferred_seen = false;

  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const auto &candidate = candidates[index];
    const bool preferred = !requirements.preferred_transport.empty() &&
                           candidate.name == requirements.preferred_transport;
    preferred_seen = preferred_seen || (preferred && candidate.available);
    if (requirements.require_preferred_transport && !preferred) continue;
    const auto compatibility = IsCompatible(candidate, requirements);
    if (!compatibility.compatible) continue;

    const auto rank = std::make_tuple(
        candidate.priority, preferred, CapabilityScore(candidate.capabilities),
        std::numeric_limits<std::size_t>::max() - index);
    if (selected == std::numeric_limits<std::size_t>::max() ||
        rank > selected_rank) {
      selected = index;
      selected_staging = compatibility.uses_staging;
      selected_rank = rank;
    }
  }

  if (selected != std::numeric_limits<std::size_t>::max())
    return {selected, SelectionError::None, selected_staging};
  if (requirements.require_preferred_transport && !preferred_seen)
    return {{}, SelectionError::PreferredTransportUnavailable, false};
  return {{}, SelectionError::NoCompatibleTransport, false};
}

const char *ToString(SelectionError error) {
  switch (error) {
    case SelectionError::None: return "none";
    case SelectionError::NoCandidates: return "no transport candidates";
    case SelectionError::NoCompatibleTransport: return "no compatible transport";
    case SelectionError::PreferredTransportUnavailable:
      return "preferred transport unavailable";
  }
  return "unknown selection error";
}

}  // namespace gvirtus::transport::v2
