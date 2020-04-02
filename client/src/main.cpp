#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <connection.hpp>
#include <iostream>
#include <list>
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using asio::ip::tcp;

//
class Replace
{
public:
  std::regex  reg;
  std::string fmt;

  std::string replace(const std::string str) const
  {
    const auto flags = std::regex_constants::format_sed;
    return std::regex_replace(str, reg, fmt, flags);
  }
};
std::vector<Replace> replace_list;

//
//
//
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
    tcp::resolver::query query(server_name_, "33001");
    resolver_.async_resolve(
        query, [&](auto& err, auto iter) { on_resolve(err, iter); });
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
    asio::async_connect(socket_, endpoint_iterator,
                        [&](auto& err, auto i) { on_connect(err); });
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
          for (const auto& r : replace_list)
          {
            b = r.replace(b);
          }
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

  std::string         hostname;
  Network::BufferList buff_list;
  int                 start_cmd = 1;
  for (; start_cmd < argc; start_cmd++)
  {
    std::string a = argv[start_cmd];
    if (hostname.empty())
    {
      // ホスト名の指定よりも前
      if (a == "-p")
      {
        // 正規表現パターン
        if (++start_cmd >= argc)
        {
          break;
        }
        std::string            p = argv[start_cmd];
        std::string            d{p[0]};
        std::list<std::string> rl;
        boost::split(rl, p, boost::is_any_of(d));
        if (rl.size() < 3)
        {
          break;
        }
        Replace r;
        auto    rit = rl.begin();
        r.reg       = *(++rit);
        r.fmt       = *(++rit);
        replace_list.push_back(r);
      }
      else
      {
        hostname = argv[start_cmd];
      }
    }
    else
    {
      //
      buff_list.push_back(argv[start_cmd]);
    }
  }

  if (buff_list.empty())
  {
    std::cerr << argv[0]
              << ": [option] <hostname> command line...\n"
                 "[option]\n"
                 "\t-p pattern\treplace the console output by regex\n"
              << std::endl;
    return 1;
  }

  auto w  = std::make_shared<asio::io_service::work>(io_service);
  auto th = std::thread([&]() { io_service.run(); });
  client.start(hostname);
  while (client.isConnect() == false)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  client.send(buff_list);
  while (client.isFinished() == false)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  w.reset();
  th.join();
}
