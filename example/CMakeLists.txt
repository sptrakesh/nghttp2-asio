add_executable(server server.cpp)
target_include_directories(server PRIVATE ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
target_include_directories(server INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/../lib/includes"
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/includes"
)
target_link_libraries(server PRIVATE nghttp2_asio_static ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})

add_executable(client client.cpp)
target_include_directories(client PRIVATE ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
target_include_directories(client INTERFACE
    "${CMAKE_CURRENT_BINARY_DIR}/../lib/includes"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/includes"
)
target_link_libraries(client PRIVATE nghttp2_asio_static ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})
