//
// Created by Rakesh on 27/12/2024.
//

#include <iostream>

#include <nghttp2/asio_http2_client.h>

using boost::asio::ip::tcp;

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::client;

int main()
{
  boost::asio::io_context io_context;

  // connect to localhost:3000
  session sess(io_context, "localhost", "3000");

  sess.on_connect([&sess](const tcp::endpoint&) {
    boost::system::error_code ec;

    auto req = sess.submit(ec, "GET", "http://localhost:3000/");

    req->on_response([](const response &res) {
      // print status code and response header fields.
      std::cerr << "HTTP/2 " << res.status_code() << std::endl;
      for (auto &kv : res.header()) {
        std::cerr << kv.first << ": " << kv.second.value << "\n";
      }
      std::cerr << std::endl;

      res.on_data([](const uint8_t *data, std::size_t len) {
        std::cerr.write(reinterpret_cast<const char *>(data), len);
        std::cerr << std::endl;
      });
    });

    req->on_close([&sess](uint32_t error_code) {
      // shutdown session after first request was done.
      sess.shutdown();
    });
  });

  sess.on_error([](const boost::system::error_code &ec) {
    std::cerr << "error: " << ec.message() << std::endl;
  });

  io_context.run();
}
