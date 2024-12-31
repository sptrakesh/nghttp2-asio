add_executable(server server.cpp)
target_include_directories(server PRIVATE ${Boost_INCLUDE_DIRS} ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})

if(ENABLE_SHARED_LIB)
  target_link_libraries(server PRIVATE nghttp2_asio ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})
else ()
  target_link_libraries(server PRIVATE nghttp2_asio_static ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})
endif (ENABLE_SHARED_LIB)

add_executable(client client.cpp)
target_include_directories(client PRIVATE ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})

if(ENABLE_SHARED_LIB)
  target_link_libraries(client PRIVATE nghttp2_asio ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})
else ()
  target_link_libraries(client PRIVATE nghttp2_asio_static ${LIBNGHTTP2_LIBRARIES} ${OPENSSL_LIBRARIES})
endif (ENABLE_SHARED_LIB)
