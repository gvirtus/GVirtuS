if(NOT WIN32)
    add_executable(tcp-communicator-test
            transport/TcpCommunicatorTest.cpp
            ${PROJECT_SOURCE_DIR}/src/communicators/tcp/TcpCommunicator.cpp)
    target_include_directories(tcp-communicator-test PRIVATE
            ${PROJECT_SOURCE_DIR}/src/communicators/tcp)
    target_compile_definitions(tcp-communicator-test PRIVATE
            GVIRTUS_TCP_NO_PLUGIN_FACTORY)
    add_test(NAME tcp-communicator-lifecycle COMMAND tcp-communicator-test)
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
