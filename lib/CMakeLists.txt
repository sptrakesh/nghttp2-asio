add_subdirectory(includes)

file(GLOB c_sources *.c)
set_source_files_properties(${c_sources} PROPERTIES
  COMPILE_FLAGS "${WARNCFLAGS}")
file(GLOB cxx_sources *.cc)
set_source_files_properties(${cxx_sources} PROPERTIES
  COMPILE_FLAGS "${WARNCXXFLAGS} ${CXX1XCXXFLAGS}")

set(NGHTTP2_ASIO_SOURCES
  util.cc
  http2.cc
  timegm.c
  asio_common.cc
  asio_io_service_pool.cc
  asio_server_http2.cc
  asio_server_http2_impl.cc
  asio_server.cc
  asio_server_http2_handler.cc
  asio_server_request.cc
  asio_server_request_impl.cc
  asio_server_response.cc
  asio_server_response_impl.cc
  asio_server_stream.cc
  asio_server_serve_mux.cc
  asio_server_request_handler.cc
  asio_server_tls_context.cc
  asio_client_session.cc
  asio_client_session_impl.cc
  asio_client_session_tcp_impl.cc
  asio_client_session_tls_impl.cc
  asio_client_response.cc
  asio_client_response_impl.cc
  asio_client_request.cc
  asio_client_request_impl.cc
  asio_client_stream.cc
  asio_client_tls_context.cc
)

if (NOT ENABLE_SHARED_LIB AND NOT ENABLE_STATIC_LIB)
  if (BUILD_SHARED_LIBS)
    set(ENABLE_SHARED_LIB ON)
  else()
    set(ENABLE_STATIC_LIB ON)
  endif()
endif()

set(Target_Name "nghttp2_asio")
if (ENABLE_SHARED_LIB)
  add_library(${Target_Name} SHARED ${NGHTTP2_ASIO_SOURCES})
endif (ENABLE_SHARED_LIB)
if (ENABLE_STATIC_LIB)
  add_library(${Target_Name} STATIC ${NGHTTP2_ASIO_SOURCES})
endif (ENABLE_STATIC_LIB)
add_library(nghttp2::asio ALIAS ${Target_Name})

target_compile_definitions(${Target_Name}
  PRIVATE _FORTIFY_SOURCE=2
)

target_include_directories(${Target_Name}
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/includes"
  ${LIBNGHTTP2_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/includes>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${Target_Name}
  PRIVATE
  ${LIBNGHTTP2_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${Boost_LIBRARIES}
  Boost::url
)
target_link_libraries(${Target_Name} INTERFACE  ${LIBNGHTTP2_LIBRARIES} ${Boost_LIBRARIES} Boost::url ${OPENSSL_LIBRARIES})
set_target_properties(${Target_Name}
  PROPERTIES
  VERSION ${LT_VERSION}
  SOVERSION ${LT_SOVERSION}
)
target_compile_features(${Target_Name} INTERFACE cxx_std_23)

install(TARGETS ${Target_Name} EXPORT Nghttp2AsioTargets DESTINATION lib)
install(TARGETS ${Target_Name} DESTINATION lib)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libnghttp2_asio.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
install(EXPORT Nghttp2AsioTargets
    FILE Nghttp2AsioTargets.cmake
    NAMESPACE nghttp2::
    DESTINATION lib/cmake/Nghttp2Asio
)
