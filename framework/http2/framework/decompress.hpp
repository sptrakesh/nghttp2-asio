//
// Created by Rakesh on 25/02/2025.
// https://github.com/mapbox/gzip-hpp/blob/master/include/gzip/decompress.hpp
//

#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include <zlib.h>

#ifndef ZLIB_CONST
#define ZLIB_CONST
#endif

namespace spt::http2::framework
{
  namespace detail
  {
    struct Decompressor
    {
      Decompressor(
        std::size_t maxBytes = 1000000000 // by default refuse operation if compressed data is > 1GB
        ) : max{ maxBytes } {}

      template <typename OutputType>
      void decompress( OutputType& output, const char* data, std::size_t size ) const
      {
        z_stream inflates;

        inflates.zalloc = Z_NULL;
        inflates.zfree = Z_NULL;
        inflates.opaque = Z_NULL;
        inflates.avail_in = 0;
        inflates.next_in = Z_NULL;

        constexpr int windowBits = 15 + 32; // auto with windowbits of 15
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        if ( inflateInit2( &inflates, windowBits ) != Z_OK ) throw std::runtime_error( "inflate init failed" );
#pragma GCC diagnostic pop
        inflates.next_in = reinterpret_cast<Bytef*>( const_cast<char*>( data ) );

#ifdef DEBUG
        // Verify if size (long type) input will fit into unsigned int, type used for zlib's avail_in
        std::uint64_t size64 = size * 2;
        if ( size64 > std::numeric_limits<unsigned int>::max() )
        {
          inflateEnd( &inflates );
          throw std::runtime_error( "size arg is too large to fit into unsigned int type x2" );
        }
#endif

        if ( size > max || (size * 2) > max )
        {
          inflateEnd( &inflates );
          throw std::runtime_error( "size may use more memory than intended when decompressing" );
        }

        inflates.avail_in = static_cast<unsigned int>(size);
        std::size_t sizeUncompressed = 0;

        do
        {
          std::size_t resizeTo = sizeUncompressed + 2 * size;
          if ( resizeTo > max )
          {
            inflateEnd( &inflates );
            throw std::runtime_error( "size of output string will use more memory then intended when decompressing" );
          }

          output.resize( resizeTo );
          inflates.avail_out = static_cast<unsigned int>( 2 * size );
          inflates.next_out = reinterpret_cast<Bytef*>( &output[0] + sizeUncompressed );
          int ret = inflate( &inflates, Z_FINISH );
          if ( ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR )
          {
            std::string err = inflates.msg;
            inflateEnd( &inflates );
            throw std::runtime_error( err );
          }

          sizeUncompressed += ( 2 * size - inflates.avail_out );
        }
        while ( inflates.avail_out == 0 );

        inflateEnd( &inflates );
        output.resize( sizeUncompressed );
      }

    private:
      std::size_t max;
    };
  }

  template <typename OutputType>
  OutputType decompress( std::string_view data )
  {
    detail::Decompressor decomp;
    std::string output;
    decomp.decompress( output, data.data(), data.size() );
    return output;
  }
}