cmake_policy(SET CMP0048 NEW)

project(Lab1-myFTP)
cmake_minimum_required(VERSION 3.10)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_library(mlib socket.c)

add_executable(ftp_server ftp_server.c)
target_include_directories(ftp_server PUBLIC include)
target_include_directories(ftp_server PUBLIC mlib)
target_compile_options(ftp_server PUBLIC -Ofast)

add_executable(ftp_client ftp_client.c)
target_compile_options(ftp_client PUBLIC -Ofast)
target_include_directories(ftp_client PUBLIC include)
target_include_directories(ftp_client PUBLIC mlib)
