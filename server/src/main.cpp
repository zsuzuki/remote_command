#include <atomic>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/process.hpp>
#include <connection.hpp>
#include <iostream>
#include <thread>
#include <vector>

namespace asio    = boost::asio;
namespace process = boost::process;
using asio::ip::tcp;

class Server : public Network::ConnectionBase
{
  tcp::acceptor acceptor_;

public:
  Server(asio::io_service& io_service)
      : Network::ConnectionBase(io_service),
        acceptor_(io_service, tcp::endpoint(tcp::v4(), 32770))
  {
  }

  void start() { start_accept(); }

private:
  // 接続待機
  void start_accept()
  {
    acceptor_.async_accept(socket_, boost::bind(&Server::on_accept, this,
                                                asio::placeholders::error));
  }

  // 接続待機完了
  void on_accept(const boost::system::error_code& error)
  {
    if (error)
    {
      std::cout << "accept failed: " << error.message() << std::endl;
      return;
    }

    start_receive([&](auto cmd, auto bufflist) {
      std::string command = cmd;
      std::cout << command << "/" << bufflist.size() << std::endl;
      if (command != "error" && bufflist.size() > 0)
      {
        for (auto& b : bufflist)
        {
          if (command.empty() == false)
          {
            command += " ";
          }
          command += b;
        }
        std::cout << "CMD: " << command << std::endl;

        process::ipstream out_stream;
        process::ipstream err_stream;
        process::child    c(command, process::std_out > out_stream,
                         process::std_err > err_stream);

        Network::BufferList blist;
        while (out_stream || err_stream)
        {
          std::string oline;
          if (std::getline(out_stream, oline) && !oline.empty())
            std::cout << oline << std::endl;
          std::string eline;
          if (std::getline(err_stream, eline) && !eline.empty())
          {
            blist.push_back(eline);
          }
        }

        c.wait();

        if (blist.empty())
        {
          blist.push_back("no error");
        }
        send("finish", blist, [&](bool) {});
      }
    });
  }
};

int
main()
{
  for (;;)
  {
    asio::io_service io_service;
    Server           server(io_service);
    server.start();
    io_service.run();
    std::cout << "done." << std::endl;
  }
}
