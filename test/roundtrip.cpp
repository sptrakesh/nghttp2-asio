//
// Created by Rakesh on 27/12/2024.
//

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <catch2/catch_test_macros.hpp>
#include <charconv>
#include <format>
#include <iostream>
#include <tuple>
#include <nghttp2/asio_http2_client.h>
#include <nghttp2/asio_http2_server.h>

namespace {
namespace ptest {

void root(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res) {
  if (req.method() != "GET") {
    res.write_head(405);
    res.end();
    return;
  }
  res.write_head(200, {{"content-type", {"text/plain", false}}});
  res.end("Ok");
}

void data(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res) {
  if (req.method() != "GET") {
    res.write_head(405);
    res.end();
    return;
  }
  res.write_head(200, {{"content-type", {"application/json", false}}});
  res.end( boost::json::serialize(boost::json::object{{"status", "ok"}}));
}

void receive(const nghttp2::asio_http2::server::request& req, const nghttp2::asio_http2::server::response& res) {
  if (req.method() != "POST") {
    res.write_head(405);
    res.end();
    return;
  }
  auto data = std::make_shared<std::string>();

  auto iter = req.header().find( "content-length" );
  if ( iter == req.header().end() ) iter = req.header().find( "Content-Length" );

  if ( iter != req.header().end() ) {
    std::size_t length{};
    auto [ptr, ec] { std::from_chars( iter->second.value.data(), iter->second.value.data() + iter->second.value.size(), length ) };
    if ( ec == std::errc() ) data->reserve( length );
  }
  else {
    std::cerr << "Content-Length header not found" << std::endl;
    data->reserve( 2048 );
  }

  req.on_data([data, &res](const uint8_t* chars, std::size_t size) {
    if (size > 0) {
      data->append(reinterpret_cast<const char*>(chars), size);
      return;
    }

    auto ec = boost::system::error_code{};
    auto parsed = boost::json::parse(*data, ec);
    if (ec) {
      std::cerr << "parse error: " << ec.message() << ". " << *data << std::endl;
      res.write_head(400, {{"content-type", {"application/json", false}}});
      res.end( boost::json::serialize(boost::json::object{{"code", 400}, {"cause", ec.message()}}));
      return;
    }

    if (!parsed.is_object()) {
      std::cerr << "Expected object\n";
      res.write_head(400, {{"content-type", {"application/json", false}}});
      res.end(boost::json::serialize( boost::json::object{{"code", 400}, {"cause", "Not object"}}));
      return;
    }

    res.write_head(200, {{"content-type", {"application/json", false}}});
    res.end(boost::json::serialize(boost::json::object{
      {"received", std::chrono::system_clock::now().time_since_epoch().count()},
      {"server", "nghttp2::asio::server"},
      {"payload", parsed.as_object()}}));
  });
}

struct Fixture {
  Fixture() {
    setUp();
  }

  ~Fixture() {
    std::cout << "Stopping server\n";
    server.stop();
    server.join();
  }

private:

  void setUp() {
    server.num_threads( 1 ); // Using pool causes assertion error.
    server.handle("/data", data);
    server.handle("/input", receive);
    server.handle("/", root);

    std::cout << "Starting HTTP/2 server on localhost:3000\n";
    boost::system::error_code ec;
    if (server.listen_and_serve(ec, "localhost", "3000", true))
    {
      std::cerr << "error: " << ec.message() << std::endl;
    }
  }

  mutable nghttp2::asio_http2::server::http2 server;
};

std::tuple<std::string, std::string> response(std::string_view path) {
  using O = std::tuple<std::string, std::string>;
  boost::asio::io_context ioc;

  auto response = std::string{};
  response.reserve(1024);
  auto ct = std::string{};

  auto s = nghttp2::asio_http2::client::session{ioc, "localhost", "3000"};
  s.on_connect([&s, &response, &ct, path](const boost::asio::ip::tcp::endpoint&) {
    boost::system::error_code ec;
    auto req = s.submit(ec, "GET", std::format("http://localhost:3000{}", path));
    if (ec) {
      std::cerr << ec.message() << std::endl;
      return;
    }

    req->on_response([&response, &ct](const nghttp2::asio_http2::client::response& res) {
      if (auto iter = res.header().find("content-type"); iter != res.header().end()) {
        ct = iter->second.value;
      }
      res.on_data([&response](const uint8_t* data, std::size_t length) {
        response.append(reinterpret_cast<const char*>(data), length);
      });
    });

    req->on_close([&s](uint32_t) {s.shutdown();});
  });

  ioc.run();
  return O{ct, response};
}

std::tuple<std::string, std::string> response(std::string_view path, const std::string& data) {
  using O = std::tuple<std::string, std::string>;
  boost::asio::io_context ioc;

  auto response = std::string{};
  response.reserve(1024);
  auto ct = std::string{};

  auto s = nghttp2::asio_http2::client::session{ioc, "localhost", "3000"};
  s.on_connect([&s, &response, &ct, path, &data](const boost::asio::ip::tcp::endpoint&) {
    boost::system::error_code ec;
    auto start = std::make_shared<std::size_t>(0);
    auto req = s.submit(ec, "POST", std::format("http://localhost:3000{}", path),
      [&data, start](uint8_t *buf, size_t len, uint32_t *data_flags) -> nghttp2::asio_http2::generator_cb::result_type {
        if (*start >= data.size()) {
          *data_flags |= NGHTTP2_DATA_FLAG_EOF;
          return 0;
        }

        const auto n = std::min<size_t>(len, data.size() - *start);
        std::memcpy(buf, data.data() + *start, n);
        *start += n;
        if (*start < data.size()) {
          *data_flags |= NGHTTP2_ERR_DEFERRED;
        } else {
          *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        }

        return n;
    }, {
      {"content-length", {std::to_string(data.size()), false}},
      {"content-type", {"application/json", false}}
    });
    if (ec) {
      std::cerr << ec.message() << std::endl;
      return;
    }

    req->on_response([&response, &ct](const nghttp2::asio_http2::client::response& res) {
      if (auto iter = res.header().find("content-type"); iter != res.header().end()) {
        ct = iter->second.value;
      }
      res.on_data([&response](const uint8_t* data, std::size_t length) {
        response.append(reinterpret_cast<const char*>(data), length);
      });
    });

    req->on_close([&s](uint32_t) {s.shutdown();});
  });

  ioc.run();
  return O{ct, response};
}

}
}

TEST_CASE_PERSISTENT_FIXTURE(ptest::Fixture, "Testing server using client", "[roundtrip]") {
  GIVEN("A server running on localhost:3000") {
    WHEN("Making get request to root path") {
      const auto [ct, resp] = ptest::response("/");
      CHECK(ct == "text/plain");
      CHECK(resp == "Ok");
    }

    AND_WHEN("Making get request to data path") {
      const auto [ct, resp] = ptest::response("/data");
      CHECK(ct == "application/json");
      REQUIRE_FALSE(resp.empty());

      auto ec = boost::system::error_code{};
      auto parsed = boost::json::parse(resp, ec);
      REQUIRE_FALSE(ec);
      REQUIRE(parsed.is_object());
      REQUIRE(parsed.as_object().contains("status"));
      REQUIRE(parsed.as_object().at("status").is_string());
      REQUIRE(parsed.as_object().at("status").as_string() == "ok");
    }

    AND_WHEN("Making post request to input path") {
      const auto json = boost::json::object{
          {"now", std::chrono::system_clock::now().time_since_epoch().count()},
          {"string", "value"},
          {"nested", boost::json::object{{"integer", 1234}, {"number", 1234.5678}}},
          {"client", "nghttp2::asio::client"}};
      const auto [ct, resp] = ptest::response("/input", boost::json::serialize(json));
      CHECK(ct == "application/json");
      REQUIRE_FALSE(resp.empty());

      auto ec = boost::system::error_code{};
      auto parsed = boost::json::parse(resp, ec);
      REQUIRE_FALSE(ec);
      REQUIRE(parsed.is_object());

      auto& obj = parsed.as_object();
      REQUIRE(obj.contains("received"));
      REQUIRE(obj.at("received").is_int64());
      CHECK(obj.at("received").as_int64() > json.at("now").as_int64());
      REQUIRE(obj.contains("server"));
      REQUIRE(obj.at("server").is_string());
      REQUIRE(obj.contains("payload"));
      REQUIRE(obj.at("payload").is_object());
      CHECK(obj.at("payload").as_object() == json);
    }

    AND_WHEN("Making request in a loop") {
      for (auto i = 0; i < 100; i++) {
        INFO(std::format("Request {}", i));
        const auto json = boost::json::object{
            {"now", std::chrono::system_clock::now().time_since_epoch().count()},
            {"string", "value"},
            {"nested", boost::json::object{{"integer", 1234}, {"number", 1234.5678}}},
            {"client", "nghttp2::asio::client"}};
        const auto [ct, resp] = ptest::response("/input", boost::json::serialize(json));
        CHECK(ct == "application/json");
        REQUIRE_FALSE(resp.empty());

        auto ec = boost::system::error_code{};
        auto parsed = boost::json::parse(resp, ec);
        REQUIRE_FALSE(ec);
        REQUIRE(parsed.is_object());

        auto& obj = parsed.as_object();
        REQUIRE(obj.contains("received"));
        REQUIRE(obj.at("received").is_int64());
        CHECK(obj.at("received").as_int64() > json.at("now").as_int64());
        REQUIRE(obj.contains("server"));
        REQUIRE(obj.at("server").is_string());
        REQUIRE(obj.contains("payload"));
        REQUIRE(obj.at("payload").is_object());
        CHECK(obj.at("payload").as_object() == json);
      }
    }
  }
}
