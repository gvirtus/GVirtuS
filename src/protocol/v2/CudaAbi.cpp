#include "gvirtus/protocol/v2/CudaAbi.h"

#include <limits>

namespace gvirtus::protocol::v2 {
namespace {

void Store16(std::byte *output, std::uint16_t value) {
  output[0] = static_cast<std::byte>((value >> 8U) & 0xffU);
  output[1] = static_cast<std::byte>(value & 0xffU);
}

void Store32(std::byte *output, std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i)
    output[i] = static_cast<std::byte>((value >> (24U - 8U * i)) & 0xffU);
}

void Store64(std::byte *output, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i)
    output[i] = static_cast<std::byte>((value >> (56U - 8U * i)) & 0xffU);
}

std::uint16_t Load16(const std::byte *input) {
  return (static_cast<std::uint16_t>(input[0]) << 8U) |
         static_cast<std::uint16_t>(input[1]);
}

std::uint32_t Load32(const std::byte *input) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < 4; ++i)
    value = (value << 8U) | static_cast<std::uint32_t>(input[i]);
  return value;
}

std::uint64_t Load64(const std::byte *input) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < 8; ++i)
    value = (value << 8U) | static_cast<std::uint64_t>(input[i]);
  return value;
}

bool IsPowerOfTwo(std::uint32_t value) {
  return value != 0 && (value & (value - 1U)) == 0;
}

bool IsValidParameterKind(KernelParameterKind kind) {
  const auto value = static_cast<std::uint8_t>(kind);
  return value >= static_cast<std::uint8_t>(KernelParameterKind::Scalar) &&
         value <=
             static_cast<std::uint8_t>(KernelParameterKind::StructuredValue);
}

}  // namespace

AbiError Validate(const Dim3 &dimensions) {
  return dimensions.x == 0 || dimensions.y == 0 || dimensions.z == 0
             ? AbiError::InvalidDimension
             : AbiError::None;
}

AbiError Validate(const PitchedPointer &pointer) {
  if (pointer.pointer == 0 || pointer.pitch == 0 || pointer.x_size == 0 ||
      pointer.y_size == 0 || pointer.pitch < pointer.x_size)
    return AbiError::InvalidEndpoint;
  if (pointer.y_size >
      std::numeric_limits<std::uint64_t>::max() / pointer.pitch)
    return AbiError::InvalidEndpoint;
  return AbiError::None;
}

AbiError Validate(const ChannelDescriptor &descriptor) {
  if (descriptor.x_bits < 0 || descriptor.y_bits < 0 ||
      descriptor.z_bits < 0 || descriptor.w_bits < 0)
    return AbiError::InvalidEndpoint;
  const auto bits = static_cast<std::uint64_t>(descriptor.x_bits) +
                    static_cast<std::uint64_t>(descriptor.y_bits) +
                    static_cast<std::uint64_t>(descriptor.z_bits) +
                    static_cast<std::uint64_t>(descriptor.w_bits);
  return bits == 0 || bits > 128 ? AbiError::InvalidEndpoint : AbiError::None;
}

AbiError Validate(const MemoryEndpoint &endpoint) {
  const auto kind = static_cast<std::uint8_t>(endpoint.kind);
  if (kind < static_cast<std::uint8_t>(MemoryEndpointKind::HostPayload) ||
      kind > static_cast<std::uint8_t>(MemoryEndpointKind::Array))
    return AbiError::InvalidEndpointKind;
  if (endpoint.identity == 0 || endpoint.byte_length == 0)
    return AbiError::InvalidEndpoint;
  if (endpoint.byte_offset >
      std::numeric_limits<std::uint64_t>::max() - endpoint.byte_length)
    return AbiError::InvalidEndpoint;
  return AbiError::None;
}

AbiError Validate(const KernelParameterDescriptor &parameter) {
  if (!IsValidParameterKind(parameter.kind))
    return AbiError::InvalidParameterKind;
  if (parameter.size == 0) return AbiError::InvalidParameterSize;
  if (!IsPowerOfTwo(parameter.alignment))
    return AbiError::InvalidParameterAlignment;
  if (parameter.offset % parameter.alignment != 0)
    return AbiError::InvalidParameterOffset;
  if (parameter.offset >
      std::numeric_limits<std::uint32_t>::max() - parameter.size)
    return AbiError::InvalidParameterOffset;
  return AbiError::None;
}

AbiError Validate(const LaunchConfiguration &launch) {
  if (launch.abi_version != kCudaAbiVersion)
    return AbiError::UnsupportedVersion;
  if (const auto error = Validate(launch.grid); error != AbiError::None)
    return error;
  return Validate(launch.block);
}

std::array<std::byte, kWireLaunchConfigurationSize> Encode(
    const LaunchConfiguration &launch) {
  std::array<std::byte, kWireLaunchConfigurationSize> wire{};
  Store16(wire.data(), launch.abi_version);
  Store32(wire.data() + 4, launch.grid.x);
  Store32(wire.data() + 8, launch.grid.y);
  Store32(wire.data() + 12, launch.grid.z);
  Store32(wire.data() + 16, launch.block.x);
  Store32(wire.data() + 20, launch.block.y);
  Store32(wire.data() + 24, launch.block.z);
  Store64(wire.data() + 32, launch.shared_memory_bytes);
  Store64(wire.data() + 40, launch.stream);
  Store32(wire.data() + 48, launch.parameter_count);
  return wire;
}

LaunchDecodeResult DecodeLaunch(const std::byte *wire,
                                std::size_t wire_size) {
  if (wire == nullptr || wire_size < kWireLaunchConfigurationSize)
    return {{}, AbiError::Truncated};
  if (Load16(wire + 2) != 0 || Load32(wire + 28) != 0 ||
      Load32(wire + 52) != 0)
    return {{}, AbiError::InvalidReservedField};
  LaunchConfiguration launch;
  launch.abi_version = Load16(wire);
  launch.grid = {Load32(wire + 4), Load32(wire + 8), Load32(wire + 12)};
  launch.block = {Load32(wire + 16), Load32(wire + 20), Load32(wire + 24)};
  launch.shared_memory_bytes = Load64(wire + 32);
  launch.stream = Load64(wire + 40);
  launch.parameter_count = Load32(wire + 48);
  return {launch, Validate(launch)};
}

std::array<std::byte, kWireKernelParameterSize> Encode(
    const KernelParameterDescriptor &parameter) {
  std::array<std::byte, kWireKernelParameterSize> wire{};
  Store32(wire.data(), parameter.ordinal);
  Store32(wire.data() + 4, parameter.offset);
  Store32(wire.data() + 8, parameter.size);
  Store32(wire.data() + 12, parameter.alignment);
  wire[16] = static_cast<std::byte>(parameter.kind);
  return wire;
}

ParameterDecodeResult DecodeKernelParameter(const std::byte *wire,
                                            std::size_t wire_size) {
  if (wire == nullptr || wire_size < kWireKernelParameterSize)
    return {{}, AbiError::Truncated};
  for (std::size_t index = 17; index < kWireKernelParameterSize; ++index) {
    if (wire[index] != std::byte{0})
      return {{}, AbiError::InvalidReservedField};
  }
  KernelParameterDescriptor parameter;
  parameter.ordinal = Load32(wire);
  parameter.offset = Load32(wire + 4);
  parameter.size = Load32(wire + 8);
  parameter.alignment = Load32(wire + 12);
  parameter.kind = static_cast<KernelParameterKind>(wire[16]);
  return {parameter, Validate(parameter)};
}

std::array<std::byte, kWireMemoryEndpointSize> Encode(
    const MemoryEndpoint &endpoint) {
  std::array<std::byte, kWireMemoryEndpointSize> wire{};
  wire[0] = static_cast<std::byte>(endpoint.kind);
  Store64(wire.data() + 8, endpoint.identity);
  Store64(wire.data() + 16, endpoint.byte_offset);
  Store64(wire.data() + 24, endpoint.byte_length);
  return wire;
}

EndpointDecodeResult DecodeMemoryEndpoint(const std::byte *wire,
                                          std::size_t wire_size) {
  if (wire == nullptr || wire_size < kWireMemoryEndpointSize)
    return {{}, AbiError::Truncated};
  for (std::size_t index = 1; index < 8; ++index) {
    if (wire[index] != std::byte{0})
      return {{}, AbiError::InvalidReservedField};
  }
  MemoryEndpoint endpoint;
  endpoint.kind = static_cast<MemoryEndpointKind>(wire[0]);
  endpoint.identity = Load64(wire + 8);
  endpoint.byte_offset = Load64(wire + 16);
  endpoint.byte_length = Load64(wire + 24);
  return {endpoint, Validate(endpoint)};
}

const char *ToString(AbiError error) {
  switch (error) {
    case AbiError::None: return "none";
    case AbiError::Truncated: return "truncated typed ABI value";
    case AbiError::UnsupportedVersion: return "unsupported CUDA ABI version";
    case AbiError::InvalidDimension: return "launch dimensions must be non-zero";
    case AbiError::InvalidEndpointKind: return "invalid memory endpoint kind";
    case AbiError::InvalidEndpoint: return "invalid memory endpoint";
    case AbiError::InvalidParameterKind: return "invalid kernel parameter kind";
    case AbiError::InvalidParameterSize: return "parameter size must be non-zero";
    case AbiError::InvalidParameterAlignment: return "parameter alignment must be a power of two";
    case AbiError::InvalidParameterOffset: return "parameter offset is invalid or misaligned";
    case AbiError::InvalidReservedField: return "reserved field is not zero";
  }
  return "unknown typed ABI error";
}

}  // namespace gvirtus::protocol::v2
