cmake_minimum_required (VERSION 3.5)

project (PBRTParser)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED on)
# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# compile flags
if(APPLE)
    add_definitions(-Wno-missing-braces)
endif(APPLE)
if(WIN32)
    add_definitions(/bigobj /D_CRT_SECURE_NO_WARNINGS /wd4018 /wd4244 /wd4305 /wd4800 /wd4267)
endif(WIN32)

add_library(yocto STATIC
    yocto/ext/json.hpp
    yocto/ext/stb_image.h
    yocto/ext/nanosvg.cpp
    yocto/ext/stb_image.cpp
    yocto/ext/stb_image_write.h
    yocto/ext/stb_image_resize.h
    yocto/ext/tinyexr.h
    yocto/yocto_gltf.h
    yocto/yocto_gltf.cpp
    yocto/yocto_gl.h
    yocto/yocto_gl.cpp)
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(yocto Threads::Threads)
endif(UNIX AND NOT APPLE)

add_library(mylib STATIC

    src/PBRTParser.h
    src/utils.h
    src/PLYParser.h
    src/PBRTLexer.h
    src/spectrum.h
    src/spectrum.cpp
    src/PBRTParser.cpp
    src/utils.cpp
    src/PLYParser.cpp
    src/PBRTLexer.cpp)

add_executable(parse src/main.cpp)
target_link_libraries(parse mylib)
target_link_libraries(mylib yocto)
