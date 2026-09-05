#pragma once

#include <cstdint>

#include "gvirtus/protocol/v2/CudaAbi.h"

namespace gvirtus::protocol::v2 {

enum class CudartMemoryOperation : std::uint32_t {
  Malloc3D = 1,
  Memcpy2D = 2,
  Memcpy2DAsync = 3,
  Memcpy3D = 4,
  Memcpy3DAsync = 5,
};

enum class CopyDirection : std::uint8_t {
  HostToHost = 1,
  HostToDevice = 2,
  DeviceToHost = 3,
  DeviceToDevice = 4,
  Default = 5,
};

struct Malloc3DRequest {
  Extent3D extent{};
  std::uint32_t device = 0;
};

struct Copy3DRequest {
  MemoryEndpoint source{};
  MemoryEndpoint destination{};
  Position3D source_position{};
  Position3D destination_position{};
  Extent3D extent{};
  CopyDirection direction = CopyDirection::Default;
  VirtualObject stream = 0;
  bool asynchronous = false;
};

enum class CudartMemoryError {
  None,
  InvalidExtent,
  ExtentOverflow,
  InvalidDirection,
  EndpointMismatch,
  InvalidSource,
  InvalidDestination,
  EndpointTooSmall,
  UnsupportedPosition,
  AddressOverflow,
  MemoryTranslationFailed,
  ObjectTranslationFailed,
};

struct ResolvedMemoryEndpoint {
  MemoryEndpointKind kind = MemoryEndpointKind::HostPayload;
  std::uint64_t backend_identity = 0;
  std::uint64_t byte_offset = 0;
  std::uint64_t byte_length = 0;
};

struct EndpointTranslationResult {
  ResolvedMemoryEndpoint endpoint{};
  CudartMemoryError error = CudartMemoryError::None;
  explicit operator bool() const { return error == CudartMemoryError::None; }
};

CudartMemoryError Validate(const Malloc3DRequest &request);
CudartMemoryError Validate(const Copy3DRequest &request);

EndpointTranslationResult TranslateEndpoint(
    const MemoryEndpoint &endpoint, const VirtualMemoryRegistry &memory,
    const VirtualObjectRegistry &objects);

const char *ToString(CudartMemoryError error);

}  // namespace gvirtus::protocol::v2
