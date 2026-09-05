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
