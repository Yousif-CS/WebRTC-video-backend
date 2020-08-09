cmake_minimum_required(VERSION 3.10.2)

project(rtcServer)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -std=c++14")

set(source_dir "${PROJECT_SOURCE_DIR}/src")

add_executable(main "${source_dir}/main.cpp")

target_include_directories(main PUBLIC ${source_dir}/WebRTCsrc/include/)
target_include_directories(main PUBLIC ${source_dir}/websocketpp/)
target_include_directories(main PUBLIC ${source_dir}/jwt-cpp/include/)
target_include_directories(main PUBLIC ${source_dir}/boost/boost_1_73_0/)
target_include_directories(main PUBLIC ${source_dir}/jsoncpp/include/json/)


target_include_directories(main PUBLIC ${source_dir}/WebSocketRTC/*/)

target_link_libraries(main "${source_dir}/WebRTCsrc/lib/x64/Release/libwebrtc.a")
target_link_libraries(main "${source_dir}/jsoncpp/lib/libjsoncpp.a")

