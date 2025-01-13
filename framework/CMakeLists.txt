set(Target_Name "http2_framework")

find_package(Boost REQUIRED json)

file(GLOB_RECURSE frameworkSRCS *.cpp *.hpp)
add_library(${Target_Name} STATIC ${frameworkSRCS})
add_library(spt::http2::framework ALIAS ${Target_Name})
target_compile_definitions(${Target_Name} PRIVATE _FORTIFY_SOURCE=2)
target_compile_features(${Target_Name} INTERFACE cxx_std_23)
set_target_properties(${Target_Name} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${Target_Name} PRIVATE ${LIBNGHTTP2_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})

target_include_directories(${Target_Name}
  INTERFACE
  ${LIBNGHTTP2_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIRS}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(${Target_Name} PRIVATE nghttp2::asio Boost::json)
target_link_libraries(${Target_Name} INTERFACE nghttp2::asio Boost::json)

install(TARGETS ${Target_Name} EXPORT Http2FrameworkTargets DESTINATION lib)
install (
  DIRECTORY ${CMAKE_SOURCE_DIR}/framework/
  DESTINATION include
  FILES_MATCHING PATTERN "*.hpp")
install(EXPORT Http2FrameworkTargets
  FILE Http2FrameworkTargets.cmake
  NAMESPACE spt::http2::
  DESTINATION lib/cmake/Http2Framework
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "Http2FrameworkConfigVersion.cmake"
  VERSION 1.2.0
  COMPATIBILITY AnyNewerVersion
)

install(FILES "Http2FrameworkConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/Http2FrameworkConfigVersion.cmake" DESTINATION lib/cmake/Http2Framework)
