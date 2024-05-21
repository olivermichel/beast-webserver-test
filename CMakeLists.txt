cmake_minimum_required(VERSION 3.28)
project(beast_webserver_test)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(Boost 1.74 REQUIRED)
if (Boost_FOUND)
    message(STATUS "Found boost v${Boost_VERSION}")
    message(VERBOSE "Boost_INCLUDEDIR=${Boost_INCLUDEDIR}")
    message(VERBOSE "Boost_LIBRARIES=${Boost_LIBRARIES}")
else()
    message(SEND_ERROR "Could not find boost")
endif()

pkg_search_module(OPENSSL REQUIRED openssl)
if(OPENSSL_FOUND)
    message(STATUS "Detecting libopenssl - done
   OPENSSL_INCLUDE_DIRS: ${OPENSSL_INCLUDE_DIRS}
   OPENSSL_LIBRARIES: ${OPENSSL_LIBRARIES}
   OPENSSL_LINK_LIBRARIES: ${OPENSSL_LINK_LIBRARIES}
   OPENSSL_VERSION: ${OPENSSL_VERSION}")

endif()


set(NLOHMANN_JSON_VERSION 3.10.5)

if(NOT EXISTS ${CMAKE_HOME_DIRECTORY}/vendor/nlohmann/json.hpp)
    file(DOWNLOAD
        https://github.com/nlohmann/json/releases/download/v${NLOHMANN_JSON_VERSION}/json.hpp
        ${CMAKE_HOME_DIRECTORY}/vendor/nlohmann/json.hpp)
    message(STATUS "Downloading nlohmann_json: vendor/nlohmann/json.hpp - done")
endif()


set(INJA_VERSION 3.4.0)

if (NOT EXISTS ${CMAKE_HOME_DIRECTORY}/vendor/inja/inja.h)
    file(DOWNLOAD
        https://github.com/pantor/inja/releases/download/v${INJA_VERSION}/inja.hpp
        ${CMAKE_HOME_DIRECTORY}/vendor/inja/inja.hpp)
    message(STATUS "Downloading Inja: /vendor/inja/inja.h - done")
endif ()

add_executable(beast_webserver_test main.cc http_server.h http_server.cc)

target_include_directories(beast_webserver_test PRIVATE ${CMAKE_HOME_DIRECTORY}/vendor)
target_include_directories(beast_webserver_test PRIVATE  ${Boost_INCLUDE_DIRS})
target_include_directories(beast_webserver_test PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(beast_webserver_test PRIVATE ${Boost_LIBRARIES})
target_link_libraries(beast_webserver_test PRIVATE Threads::Threads)
target_link_libraries(beast_webserver_test PRIVATE ${OPENSSL_LINK_LIBRARIES})
set_target_properties(beast_webserver_test PROPERTIES LINKER_LANGUAGE CXX)