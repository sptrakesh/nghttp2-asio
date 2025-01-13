Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)

FetchContent_MakeAvailable(Catch2)

find_package(Boost 1.86.0 REQUIRED json)

if (APPLE)
  list(APPEND CMAKE_PREFIX_PATH "/usr/local/spt")
  list(APPEND CMAKE_PREFIX_PATH "/usr/local/mongo")
  include_directories(/usr/local/spt)
  find_package(bsoncxx REQUIRED)
else()
  list(APPEND CMAKE_PREFIX_PATH "/opt/spt")
  list(APPEND CMAKE_PREFIX_PATH "/opt/local")
  include_directories(/opt/spt)
endif (APPLE)

find_package(NanoLog REQUIRED COMPONENTS nanolog)

file(GLOB_RECURSE test_SRCS *.cpp *.h)
add_executable(integration ${test_SRCS} )

if (APPLE)
  target_include_directories(integration PRIVATE
    /usr/local/spt
    /usr/local/mongo
    ${LIBNGHTTP2_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    "${CMAKE_CURRENT_BINARY_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../framework"
  )
else()
  target_include_directories(integration PRIVATE
    /opt/spt
    /opt/local
    ${LIBNGHTTP2_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    "${CMAKE_CURRENT_BINARY_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../framework"
  )
endif (APPLE)

target_link_libraries(integration PRIVATE Catch2::Catch2 spt::http2::framework nanolog::nanolog)