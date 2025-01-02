Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)

FetchContent_MakeAvailable(Catch2)

find_package(Boost 1.86.0 REQUIRED json)

file(GLOB_RECURSE test_SRCS *.cpp *.h)
add_executable(integration ${test_SRCS} )

target_include_directories(integration PRIVATE ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS}
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/../lib/includes"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/includes"
)
target_link_libraries(integration PRIVATE Catch2::Catch2WithMain Boost::json nghttp2::asio ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})