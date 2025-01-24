find_package(Boost 1.86.0 REQUIRED json)
find_package(bsoncxx REQUIRED)

if (APPLE)
  list(APPEND CMAKE_PREFIX_PATH "/usr/local/spt")
  list(APPEND CMAKE_PREFIX_PATH "/usr/local/mongo")
  include_directories(/usr/local/spt)
else()
  list(APPEND CMAKE_PREFIX_PATH "/opt/spt")
  list(APPEND CMAKE_PREFIX_PATH "/opt/local")
  include_directories(/opt/spt)
endif (APPLE)

find_package(NanoLog REQUIRED COMPONENTS nanolog)

add_executable(server server.cpp)

if (APPLE)
  target_include_directories(server PRIVATE
    /usr/local/spt
    /usr/local/mongo
    ${LIBNGHTTP2_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    "${CMAKE_CURRENT_BINARY_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../framework"
  )
else()
  target_include_directories(server PRIVATE
    /opt/spt
    /opt/local
    ${LIBNGHTTP2_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    "${CMAKE_CURRENT_BINARY_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../framework"
  )
endif (APPLE)
target_link_libraries(server PRIVATE spt::http2::framework nanolog::nanolog)

add_executable(client client.cpp)
target_include_directories(client PRIVATE ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(client PRIVATE nghttp2::asio ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})
