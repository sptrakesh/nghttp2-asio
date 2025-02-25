//
// Created by Rakesh on 25/02/2025.
// https://github.com/mapbox/gzip-hpp/blob/master/include/gzip/compress.hpp
//

#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <zlib.h>

#ifndef ZLIB_CONST
#define ZLIB_CONST
#endif

namespace spt::http2::framework
{
  namespace detail
  {
    struct Compressor
    {
      Compressor( int level = Z_DEFAULT_COMPRESSION,
        std::size_t maxBytes = 2000000000 // by default refuse operation if uncompressed data is > 2GB
        ) : max{maxBytes}, level{level} {}

      template <typename InputType>
      void compress( InputType& output, const char* data, std::size_t size ) const
      {
#ifdef DEBUG
        // Verify if size input will fit into unsigned int, type used for zlib's avail_in
        if ( size > std::numeric_limits<unsigned int>::max() ) throw std::runtime_error( "size arg is too large to fit into unsigned int type" );
#endif

        if ( size > max ) throw std::runtime_error( "size may use more memory than intended when decompressing" );

        z_stream deflates;
        deflates.zalloc = Z_NULL;
        deflates.zfree = Z_NULL;
        deflates.opaque = Z_NULL;
        deflates.avail_in = 0;
        deflates.next_in = Z_NULL;

        constexpr int windowBits = 15 + 16; // gzip with windowbits of 15
        constexpr int memLevel = 8;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        if ( deflateInit2( &deflates, level, Z_DEFLATED, windowBits, memLevel, Z_DEFAULT_STRATEGY) != Z_OK )
        {
          throw std::runtime_error( "deflate init failed" );
        }
#pragma GCC diagnostic pop

        deflates.next_in = reinterpret_cast<Bytef*>( const_cast<char*>( data ) );
        deflates.avail_in = static_cast<unsigned int>( size );

        std::size_t sizeCompressed = 0;

        do
        {
          size_t increase = size / 2 + 1024;
          if ( output.size() < ( sizeCompressed + increase ) ) output.resize( sizeCompressed + increase );

          deflates.avail_out = static_cast<unsigned int>( increase );
          deflates.next_out = reinterpret_cast<Bytef*>( &output[0] + sizeCompressed );
          deflate( &deflates, Z_FINISH );
          sizeCompressed += ( increase - deflates.avail_out );
        }
        while ( deflates.avail_out == 0 );

        deflateEnd( &deflates );
        output.resize( sizeCompressed );
      }

    private:
      std::size_t max;
      int level;
    };
  }

  template <typename Container>
  Container compress( std::string_view data, int level = Z_DEFAULT_COMPRESSION )
  {
    detail::Compressor comp( level );
    Container output;
    comp.compress( output, data.data(), data.size() );
    return output;
  }
}