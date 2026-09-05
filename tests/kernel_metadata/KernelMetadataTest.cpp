#include "gvirtus/protocol/v2/KernelMetadata.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace gvirtus::protocol::v2;

namespace {
void Store64(std::byte *output, std::uint64_t value) {
  for (std::size_t index = 0; index < 8; ++index)
    output[index] =
        static_cast<std::byte>((value >> (56U - 8U * index)) & 0xffU);
}
}  // namespace

int main() {
  VirtualObjectRegistry objects(0x1234);
  VirtualMemoryRegistry memory(0x1234);
  const auto function = objects.Register(ObjectType::Function, 0x30000000);
  const auto stream = objects.Register(ObjectType::Stream, 0x30000010);
  const auto allocation =
      memory.Register(0x40000000, 1024, 0, AllocationType::Managed);
  assert(function && stream && allocation);

  KernelMetadata metadata;
  metadata.function = function.object.token;
  metadata.mangled_name = "_Z6kernelPvi";
  metadata.argument_size = 24;
  metadata.source = MetadataSource::FatbinaryRegistration;
  metadata.parameters = {
      {0, 0, 0, 8, 8, KernelParameterKind::DevicePointer,
       static_cast<ObjectType>(0)},
      {1, 1, 8, 8, 8, KernelParameterKind::Scalar,
       static_cast<ObjectType>(0)},
      {2, 2, 16, 8, 8, KernelParameterKind::ObjectHandle,
       ObjectType::Stream}};

  KernelMetadataRegistry registry(0x1234);
  assert(registry.Register(metadata, objects));
  assert(registry.Register(metadata, objects).error ==
         KernelMetadataError::DuplicateKernel);
  assert(registry.Lookup(metadata.function));

  std::vector<std::byte> arguments(metadata.argument_size);
  Store64(arguments.data(), allocation.allocation.virtual_base);
  Store64(arguments.data() + 8, allocation.allocation.virtual_base);
  Store64(arguments.data() + 16, stream.object.token);
  const auto prepared = PrepareKernelArguments(
      metadata, arguments.data(), arguments.size(), memory, objects);
  assert(prepared);
  assert(prepared.parameters.size() == 3);
  assert(prepared.parameters[0].translated_identity == 0x40000000);
  assert(prepared.parameters[0].managed_memory);
  assert(prepared.parameters[1].translated_identity == 0);
  assert(prepared.parameters[1].value == prepared.parameters[0].value);
  assert(prepared.parameters[2].translated_identity == 0x30000010);

  auto invalid = metadata;
  invalid.function = objects.Register(ObjectType::Function, 0x30000020).object.token;
  invalid.parameters[1].offset = 0;
  assert(registry.Register(invalid, objects).error ==
         KernelMetadataError::OverlappingParameters);

  invalid = metadata;
  invalid.function = objects.Register(ObjectType::Function, 0x30000030).object.token;
  invalid.parameters[1].index = 0;
  assert(registry.Register(invalid, objects).error ==
         KernelMetadataError::DuplicateIndex);

  arguments.resize(8);
  assert(PrepareKernelArguments(metadata, arguments.data(), arguments.size(),
                                memory, objects).error ==
         KernelMetadataError::ArgumentBlobSizeMismatch);
  assert(registry.Release(metadata.function));
  assert(registry.Lookup(metadata.function).error ==
         KernelMetadataError::UnknownKernel);
  return 0;
}
