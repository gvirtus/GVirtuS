cmake_minimum_required(VERSION 3.17)
project(GVirtuS)

option(GVIRTUS_ENABLE_RDMA "Build the legacy RDMA communicator" OFF)
option(GVIRTUS_BUILD_TESTS "Build GVirtuS unit tests" OFF)
option(GVIRTUS_BUILD_CUDA_PLUGINS "Build CUDA API frontend/backend plugins" ON)
option(GVIRTUS_BUILD_EXAMPLES "Build GVirtuS examples" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)

if("$ENV{GVIRTUS_HOME}" STREQUAL "")
    message(STATUS "Setting GVIRTUS_HOME=$ENV{HOME}/GVirtuS")
    set(GVIRTUS_HOME "$ENV{HOME}/GVirtuS")
    set($ENV{GVIRTUS_HOME} "${GVIRTUS_HOME}")
else()
    message(STATUS "Using GVIRTUS_HOME=$ENV{GVIRTUS_HOME}")
    set(GVIRTUS_HOME "$ENV{GVIRTUS_HOME}")
endif()

include(cmake/GVirtuS.cmake)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann/json.hpp
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann
        COMMAND curl -L https://github.com/nlohmann/json/releases/download/v3.7.3/json.hpp -o ${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann/json.hpp)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

add_library(gvirtus-common SHARED
        ${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann/json.hpp
        src/common/Decoder.cpp
        src/common/Encoder.cpp
        src/common/JSON.cpp
        src/common/LD_Lib.cpp
        src/common/MessageDispatcher.cpp
        src/common/Mutex.cpp
        src/common/Observable.cpp
        src/common/Observer.cpp
        src/common/SignalException.cpp
        src/common/SignalState.cpp
        ##src/communicators/rdma/ktmrdma.cpp
        src/common/Util.cpp)

add_dependencies(gvirtus-common log4cplus)

target_link_libraries(gvirtus-common stdc++fs ${CMAKE_DL_LIBS} ${LIBLOG4CPLUS})

##target_include_directories(gvirtus-common PRIVATE /usr/include/infiniband)
##target_include_directories(gvirtus-common PRIVATE /usr/include/rdma)

gvirtus_install_target(gvirtus-common)

add_library(gvirtus-protocol-v2 STATIC
        src/protocol/v2/AsyncEngine.cpp
        src/protocol/v2/CudaAbi.cpp
        src/protocol/v2/CudartMemory.cpp
        src/protocol/v2/Connection.cpp
        src/protocol/v2/Frame.cpp
        src/protocol/v2/KernelMetadata.cpp
        src/protocol/v2/ManagedMemory.cpp
        src/protocol/v2/Negotiation.cpp
        src/protocol/v2/VirtualMemory.cpp
        src/protocol/v2/VirtualObjects.cpp)
target_include_directories(gvirtus-protocol-v2 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_features(gvirtus-protocol-v2 PUBLIC cxx_std_17)
gvirtus_install_target(gvirtus-protocol-v2)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann DESTINATION ${GVIRTUS_HOME}/include)
install(DIRECTORY ${EXTERNAL_INSTALL_LOCATION}/include/log4cplus DESTINATION ${GVIRTUS_HOME}/include)
install(DIRECTORY ${EXTERNAL_INSTALL_LOCATION}/lib DESTINATION ${GVIRTUS_HOME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/etc DESTINATION ${GVIRTUS_HOME})

add_library(gvirtus-communicators SHARED
        src/communicators/Buffer.cpp
        src/communicators/CommunicatorFactory.cpp
        src/communicators/Endpoint_Tcp.cpp
        src/communicators/Endpoint_Rdma.cpp
        src/communicators/EndpointFactory.cpp
        src/communicators/Result.cpp
        )
target_link_libraries(gvirtus-communicators gvirtus-common)
gvirtus_install_target(gvirtus-communicators)

## TCP COMMUNICATOR
add_library(gvirtus-communicators-tcp SHARED
        src/communicators/tcp/TcpCommunicator.cpp)
target_link_libraries(gvirtus-communicators-tcp gvirtus-communicators)
gvirtus_install_target(gvirtus-communicators-tcp)

if(GVIRTUS_ENABLE_RDMA)
    find_path(GVIRTUS_RDMA_INCLUDE_DIR rdma/rdma_cma.h REQUIRED)
    find_library(GVIRTUS_RDMACM_LIBRARY rdmacm REQUIRED)
    find_library(GVIRTUS_IBVERBS_LIBRARY ibverbs REQUIRED)

    add_library(gvirtus-communicators-ib SHARED
            src/communicators/rdma/ktmrdma.cpp
            src/communicators/rdma/RdmaCommunicator.cpp)
    target_include_directories(gvirtus-communicators-ib PRIVATE ${GVIRTUS_RDMA_INCLUDE_DIR})
    target_link_libraries(gvirtus-communicators-ib PRIVATE
            gvirtus-communicators
            ${GVIRTUS_RDMACM_LIBRARY}
            ${GVIRTUS_IBVERBS_LIBRARY})
    gvirtus_install_target(gvirtus-communicators-ib)
endif()

add_library(gvirtus-frontend SHARED
        src/frontend/Frontend.cpp)
target_link_libraries(gvirtus-frontend gvirtus-communicators Threads::Threads)
gvirtus_install_target(gvirtus-frontend)

add_executable(gvirtus-backend
        src/backend/Backend.cpp
        src/backend/main.cpp
        src/backend/Process.cpp
        src/backend/Property.cpp)
target_link_libraries(gvirtus-backend gvirtus-communicators Threads::Threads)
gvirtus_install_target(gvirtus-backend)

if(GVIRTUS_BUILD_CUDA_PLUGINS)
    add_subdirectory(plugins/cublas)
    add_subdirectory(plugins/cudart)
    add_subdirectory(plugins/cufft)
    add_subdirectory(plugins/curand)
    add_subdirectory(plugins/cudnn)
    #add_subdirectory(plugins/cusolver)
    #add_subdirectory(plugins/cusparse)
endif()

#add_subdirectory(tools/protocol-generator)

if(GVIRTUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GVIRTUS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
