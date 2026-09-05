#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "gvirtus/protocol/v2/VirtualMemory.h"
#include "gvirtus/protocol/v2/VirtualObjects.h"

namespace gvirtus::protocol::v2 {

constexpr std::uint16_t kCudaAbiVersion = 1;
constexpr std::size_t kWireLaunchConfigurationSize = 56;
constexpr std::size_t kWireKernelParameterSize = 32;
constexpr std::size_t kWireMemoryEndpointSize = 32;

struct Dim3 {
  std::uint32_t x = 1;
  std::uint32_t y = 1;
  std::uint32_t z = 1;
};

struct Extent3D {
  std::uint64_t width = 0;
  std::uint64_t height = 0;
  std::uint64_t depth = 0;
};

struct Position3D {
  std::uint64_t x = 0;
  std::uint64_t y = 0;
  std::uint64_t z = 0;
};

struct PitchedPointer {
  VirtualAddress pointer = 0;
  std::uint64_t pitch = 0;
  std::uint64_t x_size = 0;
  std::uint64_t y_size = 0;
};

struct ChannelDescriptor {
  std::int32_t x_bits = 0;
  std::int32_t y_bits = 0;
  std::int32_t z_bits = 0;
  std::int32_t w_bits = 0;
  std::uint32_t format = 0;
};

enum class MemoryEndpointKind : std::uint8_t {
  HostPayload = 1,
  DevicePointer = 2,
  Array = 3,
};

struct MemoryEndpoint {
  MemoryEndpointKind kind = MemoryEndpointKind::HostPayload;
  std::uint64_t identity = 0;
  std::uint64_t byte_offset = 0;
  std::uint64_t byte_length = 0;
};

enum class KernelParameterKind : std::uint8_t {
  Scalar = 1,
  DevicePointer = 2,
  ObjectHandle = 3,
  StructuredValue = 4,
};

struct KernelParameterDescriptor {
  std::uint32_t index = 0;
  std::uint32_t ordinal = 0;
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
  std::uint32_t alignment = 0;
  KernelParameterKind kind = KernelParameterKind::Scalar;
  ObjectType object_type = static_cast<ObjectType>(0);
};

struct LaunchConfiguration {
  Dim3 grid{};
  Dim3 block{};
  std::uint64_t shared_memory_bytes = 0;
  VirtualObject stream = 0;
  std::uint32_t parameter_count = 0;
  std::uint16_t abi_version = kCudaAbiVersion;
};

enum class AbiError {
  None,
  Truncated,
  UnsupportedVersion,
  InvalidDimension,
  InvalidEndpointKind,
  InvalidEndpoint,
  InvalidParameterKind,
  InvalidParameterSize,
  InvalidParameterAlignment,
  InvalidParameterOffset,
  InvalidReservedField,
};

struct LaunchDecodeResult {
  LaunchConfiguration launch{};
  AbiError error = AbiError::None;
  explicit operator bool() const { return error == AbiError::None; }
};

struct ParameterDecodeResult {
  KernelParameterDescriptor parameter{};
  AbiError error = AbiError::None;
  explicit operator bool() const { return error == AbiError::None; }
};

AbiError Validate(const Dim3 &dimensions);
AbiError Validate(const PitchedPointer &pointer);
AbiError Validate(const ChannelDescriptor &descriptor);
AbiError Validate(const MemoryEndpoint &endpoint);
AbiError Validate(const KernelParameterDescriptor &parameter);
AbiError Validate(const LaunchConfiguration &launch);

std::array<std::byte, kWireLaunchConfigurationSize> Encode(
    const LaunchConfiguration &launch);
LaunchDecodeResult DecodeLaunch(const std::byte *wire, std::size_t wire_size);

std::array<std::byte, kWireKernelParameterSize> Encode(
    const KernelParameterDescriptor &parameter);
ParameterDecodeResult DecodeKernelParameter(const std::byte *wire,
                                            std::size_t wire_size);

struct EndpointDecodeResult {
  MemoryEndpoint endpoint{};
  AbiError error = AbiError::None;
  explicit operator bool() const { return error == AbiError::None; }
};

std::array<std::byte, kWireMemoryEndpointSize> Encode(
    const MemoryEndpoint &endpoint);
EndpointDecodeResult DecodeMemoryEndpoint(const std::byte *wire,
                                          std::size_t wire_size);

const char *ToString(AbiError error);

}  // namespace gvirtus::protocol::v2
