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
using child_ptr = std::shared_ptr<process::child>;

class Server : public Network::ConnectionBase
{
  struct Pipe
  {
    // process::async_pipe pipe_;
    process::ipstream stream_;
    Network::Buffer   buffer_;
    std::string       title_;

    // Pipe(asio::io_service& ios, std::string n) : pipe_(ios), title_(n)
    Pipe(std::string n) : title_(n) { buffer_.resize(8192); }
  };

  tcp::acceptor         acceptor_;
  child_ptr             child_;
  std::shared_ptr<Pipe> outpipe_;
  std::shared_ptr<Pipe> errpipe_;

public:
  Server(asio::io_service& io_service)
      : Network::ConnectionBase(io_service),
        acceptor_(io_service, tcp::endpoint(tcp::v4(), 32000))
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
      if (command != "error" && bufflist.size() > 0)
      {
        std::string command_line;
        for (auto& b : bufflist)
        {
          if (command_line.empty() == false)
          {
            command_line += " ";
          }
          command_line += b;
        }
        std::cout << "CMD: " << command_line << std::endl;

        try
        {
          outpipe_ = std::make_shared<Pipe>(/*io_service_,*/ "stdout");
          errpipe_ = std::make_shared<Pipe>(/*io_service_,*/ "stderr");
          child_   = std::make_shared<process::child>(
              command_line,
              //process::std_out > outpipe_->pipe_,
              //process::std_err > errpipe_->pipe_
              process::std_out > outpipe_->stream_,
              process::std_err > errpipe_->stream_
            );

          std::async(std::launch::async, [&]() { pipe_read(outpipe_, true); });
          std::async(std::launch::async, [&]() { pipe_read(errpipe_, false); });
        }
        catch (std::exception& e)
        {
          send("finish", {e.what()}, [&](bool) {});
        }
      }
    });
  }
  void pipe_read(std::shared_ptr<Pipe> pipe, bool send_finish)
  {
    // asio::async_read(pipe->pipe_,asio::buffer(pipe->buffer_),
    //[](const boost::system::error_code &ec, std::size_t size){});
    std::string l;
    while (pipe->stream_ && std::getline(pipe->stream_, l))
    {
      if (!l.empty())
      {
        std::cout << pipe->title_ << ": " << l << std::endl;
        send(pipe->title_.c_str(), {l}, [&](bool){});
      }
    }
    child_->wait();
    if (send_finish)
    {
      send("finish", {"no error"}, [&](bool){});
    }
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
