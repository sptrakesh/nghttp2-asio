include(CMakeFindDependencyMacro)
find_dependency(Boost 1.86 COMPONENTS json system thread url)
find_dependency(Threads)
find_dependency(ZLIB)
include(${CMAKE_CURRENT_LIST_DIR}/Nghttp2AsioTargets.cmake)
