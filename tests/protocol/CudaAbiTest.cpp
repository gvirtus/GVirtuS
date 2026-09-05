#include "gvirtus/protocol/v2/CudaAbi.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

using namespace gvirtus::protocol::v2;

int main() {
  LaunchConfiguration launch;
  launch.grid = {16, 8, 2};
  launch.block = {32, 4, 1};
  launch.shared_memory_bytes = 4096;
  launch.stream = 0x1234020000000001ULL;
  launch.parameter_count = 3;
  const auto launch_wire = Encode(launch);
  const auto decoded_launch = DecodeLaunch(launch_wire.data(), launch_wire.size());
  assert(decoded_launch);
  assert(decoded_launch.launch.grid.x == 16);
  assert(decoded_launch.launch.block.y == 4);
  assert(decoded_launch.launch.shared_memory_bytes == 4096);
  assert(decoded_launch.launch.stream == launch.stream);
  assert(decoded_launch.launch.parameter_count == 3);

  auto malformed_launch = launch_wire;
  malformed_launch[2] = std::byte{1};
  assert(DecodeLaunch(malformed_launch.data(), malformed_launch.size()).error ==
         AbiError::InvalidReservedField);
  assert(DecodeLaunch(launch_wire.data(), launch_wire.size() - 1).error ==
         AbiError::Truncated);
  launch.grid.x = 0;
  assert(Validate(launch) == AbiError::InvalidDimension);

  KernelParameterDescriptor parameter;
  parameter.ordinal = 2;
  parameter.offset = 16;
  parameter.size = 8;
  parameter.alignment = 8;
  parameter.kind = KernelParameterKind::DevicePointer;
  const auto parameter_wire = Encode(parameter);
  const auto decoded_parameter =
      DecodeKernelParameter(parameter_wire.data(), parameter_wire.size());
  assert(decoded_parameter);
  assert(decoded_parameter.parameter.kind ==
         KernelParameterKind::DevicePointer);
  assert(decoded_parameter.parameter.offset == 16);

  parameter.offset = 3;
  assert(Validate(parameter) == AbiError::InvalidParameterOffset);
  parameter.offset = 16;
  parameter.alignment = 3;
  assert(Validate(parameter) == AbiError::InvalidParameterAlignment);
  parameter.alignment = 8;
  parameter.kind = static_cast<KernelParameterKind>(99);
  assert(Validate(parameter) == AbiError::InvalidParameterKind);

  MemoryEndpoint endpoint;
  endpoint.kind = MemoryEndpointKind::DevicePointer;
  endpoint.identity = 0x1234000000010000ULL;
  endpoint.byte_offset = 128;
  endpoint.byte_length = 256;
  assert(Validate(endpoint) == AbiError::None);
  endpoint.identity = 0;
  assert(Validate(endpoint) == AbiError::InvalidEndpoint);

  PitchedPointer pitched{0x1234000000010000ULL, 1024, 768, 64};
  assert(Validate(pitched) == AbiError::None);
  pitched.pitch = 512;
  assert(Validate(pitched) == AbiError::InvalidEndpoint);

  ChannelDescriptor channel{8, 8, 8, 8, 1};
  assert(Validate(channel) == AbiError::None);
  channel.x_bits = -1;
  assert(Validate(channel) == AbiError::InvalidEndpoint);
  return 0;
}
