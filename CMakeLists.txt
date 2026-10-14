cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(MyProject)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

find_package(raylib REQUIRED)

add_executable(main 
    main.cpp
)

target_compile_options(main PRIVATE 
    -O3
    $<$<CXX_COMPILER_ID:Clang>:
    -Weverything 
    -Werror 
    -Wno-c++98-compat 
    -Wno-c++98-compat-pedantic 
    -ferror-limit=1000 
    -fsanitize=address
    -Wno-padded
>
)

target_link_options(main PRIVATE -fsanitize=address)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(main PRIVATE raylib)
set_target_properties(raylib PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ON)

if(APPLE)
    target_link_libraries(main PRIVATE 
        "-framework IOKit" 
        "-framework Cocoa" 
        "-framework OpenGL"
    )
endif()
