cmake_minimum_required(VERSION 3.1.1)

set(CMAKE_C_COMPILER gcc-7)
set(CMAKE_CXX_COMPILER g++-7)

project(identity)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -v")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

# append cmake find modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/CMakeModules/")

# TBB
find_package(TBB COMPONENTS tbb)

# pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# opencv
find_package(OpenCV REQUIRED)

# boost
find_package(Boost COMPONENTS program_options)

include_directories(
    ${PROJECT_SOURCE_DIR}/include/
    ${TBB_INCLUDE_DIR}
    ${OPENCV_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    /usr/local/cuda-8.0/targets/x86_64-linux/include/
    /usr/include/
)

link_directories(
    /usr/local/lib/
    /usr/lib/x86_64-linux-gnu/
)

add_executable(identity ${CMAKE_SOURCE_DIR}/src/identity.cpp)
#set(CMAKE_EXE_LINKER_FLAGS "-static")

target_link_libraries(
    identity
    ${TBB_LIBRARIES}
    ${OpenCV_LIBS}
    /usr/local/lib/libzmqpp.so
    /usr/lib/x86_64-linux-gnu/libzmq.so
    ${ZMQ_LIBRARIES}
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)