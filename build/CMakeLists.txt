
cmake_minimum_required (VERSION 3.1.1)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY /home/eons/git/aoegreens/srv_forward_internal_api/build/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY /home/eons/git/aoegreens/srv_forward_internal_api/build/out)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY /home/eons/git/aoegreens/srv_forward_internal_api/build/out)
project(forward_internal_api)
include_directories(/home/eons/git/aoegreens/srv_forward_internal_api/inc)
add_executable(forward_internal_api /home/eons/git/aoegreens/srv_forward_internal_api/src/main.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(forward_internal_api Threads::Threads)
target_link_libraries(forward_internal_api restbed cpr)
