//
// Created by Rakesh on 07/01/2025.
//

#include <catch2/catch_session.hpp>
#include <log/NanoLog.hpp>

int main( int argc, char* argv[] )
{
  nanolog::set_log_level( nanolog::LogLevel::DEBUG );
  nanolog::initialize( nanolog::GuaranteedLogger(), "/tmp/", "nghttp2-asio", false );
  return Catch::Session().run( argc, argv );
}
