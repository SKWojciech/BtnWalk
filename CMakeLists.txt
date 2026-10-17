cmake_minimum_required(VERSION 3.30)
project(BtnWalk CXX)

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

include(cmake/CPM.cmake)

CPMAddPackage(
	NAME LibXml2
	VERSION 2.15.4
	URL https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.4.tar.xz
	URL_HASH SHA256=98087FD181D9070724F3FBC65C7377DB03038EB92BD882374DAFF44940138821
	OPTIONS
		"LIBXML2_WITH_ICONV OFF"
		"LIBXML2_WITH_ZLIB OFF"
)

# Cannot name it "nghttp2": CPM would register a find_package redirect for that name, which
# shadows curl's FindNGHTTP2.cmake so CURL::nghttp2 never gets created.
# This still causes a warning about a 'Bad lib', but at least it configures now!
CPMAddPackage(
	NAME libnghttp2
	VERSION 1.70.0
	URL https://github.com/nghttp2/nghttp2/releases/download/v1.70.0/nghttp2-1.70.0.tar.xz
	URL_HASH SHA256=E05CB1388EACA3830ADED4CCF20044B6E1AC1A61411DCCA11B0437C4285C8BC2
	OPTIONS
		"ENABLE_LIB_ONLY ON"
)

CPMAddPackage(
	NAME Boost
	VERSION 1.92.0
	URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
	URL_HASH SHA256=9BED76128D4E46755DBE818487788C6FCEB6F72B378F4DAA49B7E1E600D9088D
	OPTIONS
		"BOOST_INCLUDE_LIBRARIES graph"
)


get_target_property(NGHTTP2_INCLUDE_DIR nghttp2 INTERFACE_INCLUDE_DIRECTORIES)
set(NGHTTP2_LIBRARY nghttp2)
if (WIN32)
	set(CURL_TLS_BACKEND "CURL_USE_SCHANNEL ON")
else()
	set(CURL_TLS_BACKEND "CURL_USE_OPENSSL ON")
endif()
CPMAddPackage(
	NAME CURL
	VERSION 8.23.0
	URL https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.xz
	URL_HASH SHA256=8DADD7D2536A314F982AD05A651B713F426BF7D02CC2C76CFFB9796F2218E6C5
	OPTIONS
		"BUILD_CURL_EXE OFF"
		"CURL_USE_LIBPSL OFF"
		"CURL_ENABLE_SSL ON"
		"BUILD_LIBCURL_DOCS OFF"
		"BUILD_MISC_DOCS OFF"
		"ENABLE_CURL_MANUAL OFF"
		"${CURL_TLS_BACKEND}"
)

CPMAddPackage(
	NAME imageinfo
	VERSION 2.0.0
	URL https://github.com/xiaozhuai/imageinfo/archive/refs/tags/v2.0.0.tar.gz
	URL_HASH SHA256=CA033B939DAE24E990389781728CE948E09665F45A2BE70D4AC1AFF83931D0EB
)

CPMAddPackage(
	NAME fmt
	VERSION 12.2.0
	URL https://github.com/fmtlib/fmt/releases/download/12.2.0/fmt-12.2.0.zip
	URL_HASH SHA256=A2F4A8D51178F954E4C339007F77EDD76BA0CB2E36F87A48E5A5403D9BE5878F
)

CPMAddPackage(
	NAME cxxopts
	VERSION 3.3.1
	URL https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.3.1.tar.gz
	URL_HASH SHA256=3BFC70542C521D4B55A46429D808178916A579B28D048BD8C727EE76C39E2072
)


add_executable(btnwalk)
target_sources(btnwalk PRIVATE main.cpp)
target_link_libraries(btnwalk PRIVATE fmt::fmt CURL::libcurl LibXml2 imageinfo::imageinfo Boost::graph cxxopts::cxxopts)
