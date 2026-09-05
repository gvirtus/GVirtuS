#include "gvirtus/protocol/v2/CudartMemory.h"

#include <limits>

namespace gvirtus::protocol::v2 {
namespace {

bool IsHost(MemoryEndpointKind kind) {
  return kind == MemoryEndpointKind::HostPayload;
}

bool IsDevice(MemoryEndpointKind kind) {
  return kind == MemoryEndpointKind::DevicePointer ||
         kind == MemoryEndpointKind::Array;
}

CudartMemoryError ValidateExtent(const Extent3D &extent) {
  if (extent.width == 0 || extent.height == 0 || extent.depth == 0)
    return CudartMemoryError::InvalidExtent;
  if (extent.height >
          std::numeric_limits<std::uint64_t>::max() / extent.width ||
      extent.depth > std::numeric_limits<std::uint64_t>::max() /
                         (extent.width * extent.height))
    return CudartMemoryError::ExtentOverflow;
  return CudartMemoryError::None;
}

}  // namespace

CudartMemoryError Validate(const Malloc3DRequest &request) {
  return ValidateExtent(request.extent);
}

CudartMemoryError Validate(const Copy3DRequest &request) {
  const auto extent_error = ValidateExtent(request.extent);
  if (extent_error != CudartMemoryError::None) return extent_error;
  if (Validate(request.source) != AbiError::None)
    return CudartMemoryError::InvalidSource;
  if (Validate(request.destination) != AbiError::None)
    return CudartMemoryError::InvalidDestination;
  if (request.source_position.x != 0 || request.source_position.y != 0 ||
      request.source_position.z != 0 || request.destination_position.x != 0 ||
      request.destination_position.y != 0 ||
      request.destination_position.z != 0)
    return CudartMemoryError::UnsupportedPosition;
  const auto bytes = request.extent.width * request.extent.height *
                     request.extent.depth;
  if (request.source.byte_length < bytes ||
      request.destination.byte_length < bytes)
    return CudartMemoryError::EndpointTooSmall;

  switch (request.direction) {
    case CopyDirection::HostToHost:
      return IsHost(request.source.kind) && IsHost(request.destination.kind)
                 ? CudartMemoryError::None
                 : CudartMemoryError::EndpointMismatch;
    case CopyDirection::HostToDevice:
      return IsHost(request.source.kind) && IsDevice(request.destination.kind)
                 ? CudartMemoryError::None
                 : CudartMemoryError::EndpointMismatch;
    case CopyDirection::DeviceToHost:
      return IsDevice(request.source.kind) && IsHost(request.destination.kind)
                 ? CudartMemoryError::None
                 : CudartMemoryError::EndpointMismatch;
    case CopyDirection::DeviceToDevice:
      return IsDevice(request.source.kind) && IsDevice(request.destination.kind)
                 ? CudartMemoryError::None
                 : CudartMemoryError::EndpointMismatch;
    case CopyDirection::Default:
      return CudartMemoryError::None;
  }
  return CudartMemoryError::InvalidDirection;
}

EndpointTranslationResult TranslateEndpoint(
    const MemoryEndpoint &endpoint, const VirtualMemoryRegistry &memory,
    const VirtualObjectRegistry &objects) {
  if (Validate(endpoint) != AbiError::None)
    return {{}, CudartMemoryError::InvalidSource};

  if (endpoint.kind == MemoryEndpointKind::HostPayload)
    return {{endpoint.kind, endpoint.identity, endpoint.byte_offset,
             endpoint.byte_length},
            CudartMemoryError::None};

  if (endpoint.kind == MemoryEndpointKind::DevicePointer) {
    if (endpoint.identity >
        std::numeric_limits<std::uint64_t>::max() - endpoint.byte_offset)
      return {{}, CudartMemoryError::AddressOverflow};
    const auto translated = memory.Resolve(
        endpoint.identity + endpoint.byte_offset, endpoint.byte_length);
    if (!translated)
      return {{}, CudartMemoryError::MemoryTranslationFailed};
    return {{endpoint.kind, translated.backend_address, 0,
             endpoint.byte_length},
            CudartMemoryError::None};
  }

  if (endpoint.byte_offset != 0)
    return {{}, CudartMemoryError::InvalidSource};
  const auto translated = objects.Resolve(endpoint.identity, ObjectType::Array);
  if (!translated)
    return {{}, CudartMemoryError::ObjectTranslationFailed};
  return {{endpoint.kind, translated.object.backend_object, 0,
           endpoint.byte_length},
          CudartMemoryError::None};
}

const char *ToString(CudartMemoryError error) {
  switch (error) {
    case CudartMemoryError::None: return "none";
    case CudartMemoryError::InvalidExtent: return "copy extent must be non-zero";
    case CudartMemoryError::ExtentOverflow: return "copy extent overflows uint64";
    case CudartMemoryError::InvalidDirection: return "invalid copy direction";
    case CudartMemoryError::EndpointMismatch: return "copy direction does not match endpoints";
    case CudartMemoryError::InvalidSource: return "invalid source endpoint";
    case CudartMemoryError::InvalidDestination: return "invalid destination endpoint";
    case CudartMemoryError::EndpointTooSmall: return "endpoint is smaller than the copy extent";
    case CudartMemoryError::UnsupportedPosition: return "non-zero copy positions require pitched layout metadata";
    case CudartMemoryError::AddressOverflow: return "endpoint address overflows";
    case CudartMemoryError::MemoryTranslationFailed: return "virtual memory translation failed";
    case CudartMemoryError::ObjectTranslationFailed: return "virtual object translation failed";
  }
  return "unknown CUDART memory error";
}

}  // namespace gvirtus::protocol::v2
