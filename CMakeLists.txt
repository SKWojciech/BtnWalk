cmake_minimum_required(VERSION 3.30)
project(BtnWalk CXX)

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

set(LIBXML2_WITH_ICONV OFF)
set(LIBXML2_WITH_ZLIB OFF)
add_subdirectory(thirdparty/libxml2)


set(ENABLE_LIB_ONLY ON)
add_subdirectory(thirdparty/nghttp2)

# Why isn't this set??
add_compile_definitions(_WIN32_WINNT=0x0A00)
add_subdirectory(thirdparty/boost)


#set(CMAKE_DISABLE_FIND_PACKAGE_NGHTTP2 ON)
#set(NGHTTP2_FOUND ON)
get_target_property(NGHTTP2_INCLUDE_DIR nghttp2 INTERFACE_INCLUDE_DIRECTORIES)
#set(NGHTTP2_INCLUDE_DIR ${nghttp2_BINARY_DIR}/lib/includes)
set(NGHTTP2_LIBRARY nghttp2)
set(BUILD_CURL_EXE OFF)
set(CURL_USE_LIBPSL OFF)
set(CURL_ENABLE_SSL ON)
set(BUILD_LIBCURL_DOCS OFF)
set(BUILD_MISC_DOCS OFF)
set(ENABLE_CURL_MANUAL OFF)
if (WIN32)
	set(CURL_USE_SCHANNEL ON)
else()
	set(CURL_USE_OPENSSL ON)
endif()
add_subdirectory(thirdparty/curl)

include(cmake/CPM.cmake)
CPMAddPackage("gh:xiaozhuai/imageinfo#5cab2dfd6d312b3acd0719ffa83cf883967ec288")
find_package(imageinfo REQUIRED)
CPMAddPackage("gh:fmtlib/fmt#12.1.0")
find_package(fmt REQUIRED)


#find_package(CURL REQUIRED)
#find_package(tidy-html5 REQUIRED)
#find_package(Boost REQUIRED COMPONENTS graph)


add_executable(btnwalk)
target_sources(btnwalk
PRIVATE
	main.cpp
)
target_link_libraries(btnwalk PRIVATE fmt::fmt CURL::libcurl LibXml2 imageinfo::imageinfo Boost::graph)
