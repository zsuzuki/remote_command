//
#pragma once

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace Network
{
namespace asio = boost::asio;
using asio::ip::tcp;

// 送受信データ
using Buffer     = std::vector<char>;
using BufferList = std::vector<std::string>;
// コールバック
using SendCallback = std::function<void(bool)>;
using RecvCallback = std::function<void(const char*, const BufferList&)>;

// 送受信ヘッダ

// 接続
class ConnectionBase
{
  struct Header
  {
    size_t length_;
    size_t count_;
    char   command_[128];
  };
  struct SendInfo
  {
    Header       header_;
    Buffer       body_;
    SendCallback callback_;
  };
  using SendInfoPtr = std::shared_ptr<SendInfo>;
  using SendQueue   = std::queue<SendInfoPtr>;

protected:
  asio::io_service& io_service_;
  tcp::socket       socket_;
  Header            read_header_;
  Buffer            read_buffer_;
  RecvCallback      read_callback_;
  SendQueue         send_que_;
  std::mutex        que_lock_;

public:
  ConnectionBase(asio::io_service& io_service)
      : io_service_(io_service), socket_(io_service)
  {
  }

  ///
  void send(const char* cmd, BufferList buff_list, SendCallback cb)
  {
    auto  info      = std::make_shared<SendInfo>();
    auto& header    = info->header_;
    auto& buffer    = info->body_;
    info->callback_ = cb;

    int total_size = 0;
    for (auto& b : buff_list)
    {
      total_size += b.size() + 1;
    }
    buffer.resize(total_size);
    int ofs = 0;
    for (auto& b : buff_list)
    {
      int n = b.size() + 1;
      strncpy(&buffer[ofs], b.c_str(), n);
      ofs += n;
    }
    strncpy(header.command_, cmd, sizeof(header.command_));
    header.length_ = buffer.size();
    header.count_  = buff_list.size();
    bool launch;
    {
      std::lock_guard<std::mutex> l(que_lock_);
      launch = send_que_.empty();
      send_que_.push(info);
    }
    if (launch)
    {
      io_service_.post([this]() { send_loop(); });
    }
  }

  ///
  void start_receive(RecvCallback cb)
  {
    read_callback_ = cb;
    boost::asio::async_read(
        socket_, asio::buffer(&read_header_, sizeof(read_header_)),
        [&](auto& err, auto bytes) { on_header_receive(err, bytes); });
  }

private:
  void on_header_receive(const boost::system::error_code& error, size_t bytes)
  {
    if (error && error != boost::asio::error::eof)
    {
      std::cout << "receive header failed: " << error.message() << std::endl;
      read_callback_("error", {});
    }
    else
    {
      read_buffer_.resize(read_header_.length_);
      boost::asio::async_read(
          socket_, asio::buffer(read_buffer_.data(), read_buffer_.size()),
          [&](auto& err, auto bytes) { on_receive(err, bytes); });
    }
  }
  void on_receive(const boost::system::error_code& error, size_t bytes)
  {
    if (error && error != boost::asio::error::eof)
    {
      std::cout << "receive failed: " << error.message() << std::endl;
      read_callback_("error", {});
    }
    else
    {
      int        ofs = 0;
      BufferList ret;
      for (int i = 0; i < read_header_.count_; i++)
      {
        ret.push_back(&read_buffer_[ofs]);
        ofs += strlen(&read_buffer_[ofs]) + 1;
      }
      read_callback_(read_header_.command_, ret);
    }
  }

  //
  void send_loop()
  {
    SendInfoPtr info;
    {
      std::lock_guard<std::mutex> l(que_lock_);
      if (!send_que_.empty())
      {
        info = send_que_.front();
        send_que_.pop();
      }
    }
    if (info)
    {
      auto& header = info->header_;
      asio::async_write(socket_, asio::buffer(&header, sizeof(header)),
                        [this, info](auto& err, auto bytes) {
                          on_send_header(info, err, bytes);
                        });
    }
  }
  void on_send_header(SendInfoPtr info, const boost::system::error_code& error,
                      size_t bytes)
  {
    if (error)
    {
      std::cerr << "error[send header]: " << error.message() << std::endl;
      info->callback_(false);
    }
    else
    {
      auto& buffer = info->body_;
      asio::async_write(
          socket_, asio::buffer(buffer),
          [this, info](auto& err, auto bytes) { on_send(info, err, bytes); });
    }
  }
  void on_send(SendInfoPtr info, const boost::system::error_code& error,
               size_t bytes)
  {
    if (error)
    {
      std::cerr << "error[send body]: " << error.message() << std::endl;
      info->callback_(false);
    }
    else
    {
      info->callback_(true);
    }
    io_service_.post([this]() { send_loop(); });
  }
};

} // namespace Network
