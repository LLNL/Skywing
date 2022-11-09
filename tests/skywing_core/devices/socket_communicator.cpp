#include <catch2/catch.hpp>

#include "skywing_core/internal/devices/socket_communicator.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

using namespace skywing;
using namespace skywing::internal;

constexpr std::uint16_t port = 40000;
constexpr int value_to_send = 3871;
std::mutex catch_mutex;

void server()
{
  SocketCommunicator conn;
  {
    std::lock_guard<std::mutex> lock{catch_mutex};
    REQUIRE(conn.set_to_listen(port) == ConnectionError::no_error);
  }
  // Twice for non-blocking/blocking connection
  for (int i = 0; i < 2; ++i) {
    const auto get_client = [&]() {
      while (true) {
        if (auto val = conn.accept()) { return std::move(*val); }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      }
    };
    // Wait for the client to connect
    SocketCommunicator with_client = get_client();
    // Wait for an int to be send
    std::array<std::byte, sizeof(value_to_send)> int_buffer;
    while (with_client.read_message(int_buffer.data(), int_buffer.size()) != ConnectionError::no_error) {
      // empty
    }
    // verify that it's the same
    int read_value;
    std::memcpy(&read_value, int_buffer.data(), sizeof(int));
    {
      std::lock_guard<std::mutex> lock{catch_mutex};
      REQUIRE(read_value == value_to_send);
    }
  }
}

void client()
{
  const auto do_send = [](SocketCommunicator& conn) {
    std::array<std::byte, sizeof(value_to_send)> int_buffer;
    std::memcpy(int_buffer.data(), &value_to_send, sizeof(value_to_send));
    std::lock_guard<std::mutex> lock{catch_mutex};
    REQUIRE(conn.send_message(int_buffer.data(), int_buffer.size()) == ConnectionError::no_error);
  };
  // Blocking
  {
    SocketCommunicator conn;
    {
      const auto res = conn.connect_to_server("127.0.0.1", port);
      std::lock_guard<std::mutex> lock{catch_mutex};
      REQUIRE(res == ConnectionError::no_error);
    }
    do_send(conn);
  }
  // Non-blocking
  {
    SocketCommunicator conn;
    {
      const auto res = conn.connect_non_blocking("127.0.0.1", port);
      std::lock_guard<std::mutex> lock{catch_mutex};
      REQUIRE(res == ConnectionError::connection_in_progress);
    }
    // Clunky work-around to actually be able to test the status
    // (This lambda in immediately invoked)
    [&]() {
      while (true) {
        const auto status = conn.connection_progress_status();
        switch (status) {
        case ConnectionError::connection_in_progress:
          std::this_thread::sleep_for(std::chrono::milliseconds{1});
          break;

        case ConnectionError::no_error:
          return;

        default:
          std::cerr << "Error returned from conn.connection_progress_status()\n";
          std::exit(1);
          break;
        }
      }
    }();
    do_send(conn);
  }
}

TEST_CASE("Communicating between sockets works", "[Skywing_SocketCommunicator]")
{
  using namespace std::chrono_literals;
  std::thread s(server);
  // Allow the server to start
  std::this_thread::sleep_for(10ms);
  std::thread c(client);
  s.join();
  c.join();
}
