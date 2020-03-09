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
using work_ptr  = std::shared_ptr<asio::io_service::work>;

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
  std::future<int>      outfuture_;
  std::future<int>      errfuture_;
  std::future<int>      childfuture_;
  work_ptr              work_;

public:
  Server(asio::io_service& io_service)
      : Network::ConnectionBase(io_service),
        acceptor_(io_service, tcp::endpoint(tcp::v4(), 32000)),
        work_(std::make_shared<asio::io_service::work>(io_service))
  {
  }

  void start() { start_accept(); }

private:
  // 接続待機
  void start_accept()
  {
    acceptor_.async_accept(socket_, [&](auto& err) { on_accept(err); });
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
              // process::std_out > outpipe_->pipe_,
              // process::std_err > errpipe_->pipe_
              process::std_out > outpipe_->stream_,
              process::std_err > errpipe_->stream_);

          outfuture_ = std::async(std::launch::async,
                                  [&]() { return pipe_read(outpipe_); });
          errfuture_ = std::async(std::launch::async,
                                  [&]() { return pipe_read(errpipe_); });
          childfuture_ =
              std::async(std::launch::async, [&]() { return child_wait(); });
        }
        catch (std::exception& e)
        {
          send("finish", {e.what()}, [&](bool) {});
        }
      }
    });
  }
  int pipe_read(std::shared_ptr<Pipe> pipe)
  {
    // asio::async_read(pipe->pipe_,asio::buffer(pipe->buffer_),
    //[](const boost::system::error_code &ec, std::size_t size){});
    std::string         l;
    int                 cnt        = 0;
    int                 total_size = 0;
    Network::BufferList bufflist;
    while (pipe->stream_ && std::getline(pipe->stream_, l))
    {
      if (!l.empty())
      {
        // std::cout << pipe->title_ << ": " << l << std::endl;
        total_size += l.size();
        bufflist.push_back(l);
        if (total_size > 500)
        {
          send(pipe->title_.c_str(), bufflist, [&](bool) {});
          bufflist.clear();
          total_size = 0;
        }
        cnt++;
      }
    }
    if (!bufflist.empty())
    {
      send(pipe->title_.c_str(), bufflist, [&](bool) {});
    }
    return cnt;
  }
  int child_wait()
  {
    child_->wait();
    outfuture_.get();
    errfuture_.get();
    send("finish", {"no error"}, [&](bool) {});
    work_.reset();
    return 0;
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
