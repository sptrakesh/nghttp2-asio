add_executable(server server.cpp)
target_include_directories(server PRIVATE ${Boost_INCLUDE_DIRS} ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(server PRIVATE nghttp2::asio ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})

add_executable(client client.cpp)
target_include_directories(client PRIVATE ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(client PRIVATE nghttp2::asio ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})
