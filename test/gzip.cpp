//
// Created by Rakesh on 25/02/2025.
//

#include <catch2/catch_test_macros.hpp>
#include <http2/framework/compress.hpp>
#include <http2/framework/decompress.hpp>
#include <nghttp2/asio_http2_server.h>

SCENARIO( "gzip test", "[gzip]" )
{
  GIVEN( "Routines for compression and decompression" )
  {
    const auto data = std::string{ "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum." };

    WHEN( "Compressing and decompressing a string" )
    {
      auto compressed = spt::http2::framework::compress<std::string>( data );
      CHECK( compressed.size() < data.size() );
      auto decompressed = spt::http2::framework::decompress<std::string>( compressed );
      CHECK( decompressed == data );
    }
  }
}
