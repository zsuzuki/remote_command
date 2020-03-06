#include <atomic>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <connection.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using asio::ip::tcp;

class Client : public Network::ConnectionBase
{
  using Super = Network::ConnectionBase;

  tcp::resolver    resolver_;
  std::string      server_name_;
  std::atomic_bool is_connect_;
  std::atomic_bool is_finished_;

public:
  Client(asio::io_service& io_service)
      : Super(io_service), resolver_(io_service), is_connect_(false),
        is_finished_(false)
  {
  }

  void start(std::string sv)
  {
    server_name_ = sv;
    connect();
  }

  // メッセージ送信
  void send(Network::BufferList& bl)
  {
    Super::send("command", bl, [&](bool s) {
      if (!s)
      {
        is_finished_ = false;
      }
    });
  }

  bool isConnect() const { return is_connect_; }
  bool isFinished() const { return is_finished_; }

private:
  void connect()
  {
    tcp::resolver::query query(server_name_, "32770");
    resolver_.async_resolve(query, boost::bind(&Client::on_resolve, this,
                                               asio::placeholders::error,
                                               asio::placeholders::iterator));
  }
  //
  void on_resolve(const boost::system::error_code& error,
                  tcp::resolver::iterator          endpoint_iterator)
  {
    if (error)
    {
      std::cout << "resolve failed: " << error.message() << std::endl;
      return;
    }
    asio::async_connect(
        socket_, endpoint_iterator,
        boost::bind(&Client::on_connect, this, asio::placeholders::error));
  }
  //
  void on_connect(const boost::system::error_code& error)
  {
    if (error)
    {
      std::cout << "connect failed : " << error.message() << std::endl;
      return;
    }
    receive();
    is_connect_ = true;
  }
  //
  void receive()
  {
    start_receive([&](auto cmd, auto buff) {
      std::string command = cmd;
      if (command != "error" && buff.size() > 0)
      {
        for (auto& b : buff)
        {
          std::cout << b << std::endl;
        }
      }
      //
      if (command == "error" || command == "finish")
      {
        // finish
        is_finished_ = true;
        std::cout << "Finished" << std::endl;
      }
      else
      {
        receive();
      }
    });
  }
};

//
//
//
int
main(int argc, char** argv)
{
  asio::io_service io_service;
  Client           client(io_service);

  if (argc <= 2)
  {
    std::cerr << argv[0] << ": <hostname> command line..." << std::endl;
    return 1;
  }
  Network::BufferList buff_list;
  for (int i = 2; i < argc; i++)
  {
    buff_list.push_back(argv[i]);
  }

  auto w  = std::make_shared<asio::io_service::work>(io_service);
  auto th = std::thread([&]() { io_service.run(); });
  client.start(argv[1]);
  while (client.isConnect() == false)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  client.send(buff_list);
  while (client.isFinished() == false)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  w.reset();
  th.join();
}
