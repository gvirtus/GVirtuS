#include "gvirtus/protocol/v2/CudartMemory.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

using namespace gvirtus::protocol::v2;

int main() {
  VirtualMemoryRegistry memory(0x1234);
  VirtualObjectRegistry objects(0x1234);
  const auto allocation = memory.Register(0x10000000, 4096);
  const auto array = objects.Register(ObjectType::Array, 0x20000000);
  assert(allocation && array);

  MemoryEndpoint device{MemoryEndpointKind::DevicePointer,
                        allocation.allocation.virtual_base, 128, 512};
  const auto wire = Encode(device);
  const auto decoded = DecodeMemoryEndpoint(wire.data(), wire.size());
  assert(decoded);
  assert(decoded.endpoint.identity == device.identity);
  assert(decoded.endpoint.byte_offset == 128);

  const auto translated_device = TranslateEndpoint(device, memory, objects);
  assert(translated_device);
  assert(translated_device.endpoint.backend_identity == 0x10000080);

  MemoryEndpoint array_endpoint{MemoryEndpointKind::Array,
                                array.object.token, 0, 512};
  const auto translated_array =
      TranslateEndpoint(array_endpoint, memory, objects);
  assert(translated_array);
  assert(translated_array.endpoint.backend_identity == 0x20000000);

  MemoryEndpoint host{MemoryEndpointKind::HostPayload, 17, 0, 512};
  Copy3DRequest copy;
  copy.source = host;
  copy.destination = device;
  copy.extent = {8, 8, 8};
  copy.direction = CopyDirection::HostToDevice;
  assert(Validate(copy) == CudartMemoryError::None);

  copy.direction = CopyDirection::DeviceToHost;
  assert(Validate(copy) == CudartMemoryError::EndpointMismatch);
  copy.source = device;
  copy.destination = host;
  assert(Validate(copy) == CudartMemoryError::None);

  copy.extent.width = 0;
  assert(Validate(copy) == CudartMemoryError::InvalidExtent);
  copy.extent = {std::numeric_limits<std::uint64_t>::max(), 2, 1};
  assert(Validate(copy) == CudartMemoryError::ExtentOverflow);
  copy.extent = {9, 8, 8};
  assert(Validate(copy) == CudartMemoryError::EndpointTooSmall);
  copy.extent = {8, 8, 8};
  copy.source_position.x = 1;
  assert(Validate(copy) == CudartMemoryError::UnsupportedPosition);

  device.byte_offset = 4000;
  device.byte_length = 128;
  assert(TranslateEndpoint(device, memory, objects).error ==
         CudartMemoryError::MemoryTranslationFailed);

  array_endpoint.byte_offset = 1;
  assert(TranslateEndpoint(array_endpoint, memory, objects).error ==
         CudartMemoryError::InvalidSource);

  auto malformed = wire;
  malformed[1] = std::byte{1};
  assert(DecodeMemoryEndpoint(malformed.data(), malformed.size()).error ==
         AbiError::InvalidReservedField);
  return 0;
}
