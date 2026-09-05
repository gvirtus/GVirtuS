if(NOT WIN32)
    add_executable(tcp-communicator-test
            transport/TcpCommunicatorTest.cpp
            ${PROJECT_SOURCE_DIR}/src/communicators/tcp/TcpCommunicator.cpp)
    target_include_directories(tcp-communicator-test PRIVATE
            ${PROJECT_SOURCE_DIR}/src/communicators/tcp)
    target_compile_definitions(tcp-communicator-test PRIVATE
            GVIRTUS_TCP_NO_PLUGIN_FACTORY)
    add_test(NAME tcp-communicator-lifecycle COMMAND tcp-communicator-test)

    add_executable(tcp-transport-v2-test transport/TcpTransportV2Test.cpp)
    target_link_libraries(tcp-transport-v2-test PRIVATE gvirtus-transport-v2)
    add_test(NAME tcp-transport-v2 COMMAND tcp-transport-v2-test)
endif()

add_executable(protocol-v2-frame-test protocol/FrameTest.cpp)
target_link_libraries(protocol-v2-frame-test PRIVATE gvirtus-protocol-v2)
add_test(NAME protocol-v2-frame COMMAND protocol-v2-frame-test)

add_executable(protocol-v2-negotiation-test protocol/NegotiationTest.cpp)
target_link_libraries(protocol-v2-negotiation-test PRIVATE gvirtus-protocol-v2)
add_test(NAME protocol-v2-negotiation COMMAND protocol-v2-negotiation-test)

add_executable(protocol-v2-connection-test protocol/ConnectionTest.cpp)
target_link_libraries(protocol-v2-connection-test PRIVATE gvirtus-protocol-v2)
add_test(NAME protocol-v2-connection COMMAND protocol-v2-connection-test)

add_executable(protocol-v2-virtual-memory-test
        virtualization/VirtualMemoryTest.cpp)
target_link_libraries(protocol-v2-virtual-memory-test PRIVATE
        gvirtus-protocol-v2)
add_test(NAME protocol-v2-virtual-memory
        COMMAND protocol-v2-virtual-memory-test)

add_executable(protocol-v2-virtual-objects-test
        virtualization/VirtualObjectsTest.cpp)
target_link_libraries(protocol-v2-virtual-objects-test PRIVATE
        gvirtus-protocol-v2)
add_test(NAME protocol-v2-virtual-objects
        COMMAND protocol-v2-virtual-objects-test)

add_executable(protocol-v2-cuda-abi-test protocol/CudaAbiTest.cpp)
target_link_libraries(protocol-v2-cuda-abi-test PRIVATE gvirtus-protocol-v2)
add_test(NAME protocol-v2-cuda-abi COMMAND protocol-v2-cuda-abi-test)

add_executable(protocol-v2-cudart-memory-test protocol/CudartMemoryTest.cpp)
target_link_libraries(protocol-v2-cudart-memory-test PRIVATE
        gvirtus-protocol-v2)
add_test(NAME protocol-v2-cudart-memory COMMAND protocol-v2-cudart-memory-test)

add_executable(protocol-v2-managed-memory-test
        unified_memory/ManagedMemoryTest.cpp)
target_link_libraries(protocol-v2-managed-memory-test PRIVATE
        gvirtus-protocol-v2)
add_test(NAME protocol-v2-managed-memory
        COMMAND protocol-v2-managed-memory-test)

add_executable(protocol-v2-kernel-metadata-test
        kernel_metadata/KernelMetadataTest.cpp)
target_link_libraries(protocol-v2-kernel-metadata-test PRIVATE
        gvirtus-protocol-v2)
add_test(NAME protocol-v2-kernel-metadata
        COMMAND protocol-v2-kernel-metadata-test)

add_executable(protocol-v2-async-engine-test protocol/AsyncEngineTest.cpp)
target_link_libraries(protocol-v2-async-engine-test PRIVATE
        gvirtus-protocol-v2)
add_test(NAME protocol-v2-async-engine COMMAND protocol-v2-async-engine-test)

add_executable(transport-v2-contract-test transport/TransportContractTest.cpp)
target_link_libraries(transport-v2-contract-test PRIVATE
        gvirtus-transport-v2 Threads::Threads)
add_test(NAME transport-v2-contract COMMAND transport-v2-contract-test)

add_custom_target(gvirtus-hardware-independent-tests DEPENDS
        tcp-communicator-test
        tcp-transport-v2-test
        protocol-v2-frame-test
        protocol-v2-negotiation-test
        protocol-v2-connection-test
        protocol-v2-virtual-memory-test
        protocol-v2-virtual-objects-test
        protocol-v2-cuda-abi-test
        protocol-v2-cudart-memory-test
        protocol-v2-managed-memory-test
        protocol-v2-kernel-metadata-test
        protocol-v2-async-engine-test
        transport-v2-contract-test)
