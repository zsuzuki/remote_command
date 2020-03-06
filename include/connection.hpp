//
#pragma once

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <functional>
#include <iostream>
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
struct Header
{
  size_t length_;
  size_t count_;
  char   command_[128];
};

// 接続
class ConnectionBase
{
protected:
  asio::io_service& io_service_;
  tcp::socket       socket_;
  Header            read_header_;
  Buffer            read_buffer_;
  RecvCallback      read_callback_;
  Header            write_header_;
  Buffer            write_buffer_;
  SendCallback      write_callback_;

public:
  ConnectionBase(asio::io_service& io_service)
      : io_service_(io_service), socket_(io_service)
  {
  }

  ///
  void send(const char* cmd, BufferList buff_list, SendCallback cb)
  {
    int total_size = 0;
    for (auto& b : buff_list)
    {
      total_size += b.size() + 1;
    }
    write_buffer_.resize(total_size);
    int ofs = 0;
    for (auto& b : buff_list)
    {
      int n = b.size() + 1;
      strncpy(&write_buffer_[ofs], b.c_str(), n);
      ofs += n;
    }
    write_callback_ = cb;
    strncpy(write_header_.command_, cmd, sizeof(write_header_.command_));
    write_header_.length_ = write_buffer_.size();
    write_header_.count_  = buff_list.size();
    asio::async_write(socket_,
                      asio::buffer(&write_header_, sizeof(write_header_)),
                      boost::bind(&ConnectionBase::on_send_header, this,
                                  asio::placeholders::error,
                                  asio::placeholders::bytes_transferred));
  }

  ///
  void start_receive(RecvCallback cb)
  {
    read_callback_ = cb;
    boost::asio::async_read(socket_,
                            asio::buffer(&read_header_, sizeof(read_header_)),
                            boost::bind(&ConnectionBase::on_header_receive,
                                        this, asio::placeholders::error,
                                        asio::placeholders::bytes_transferred));
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
      std::cout << "read_header:" << read_header_.command_ << "/"
                << read_header_.length_ << std::endl;
      read_buffer_.resize(read_header_.length_);
      boost::asio::async_read(
          socket_, asio::buffer(read_buffer_.data(), read_buffer_.size()),
          boost::bind(&ConnectionBase::on_receive, this,
                      asio::placeholders::error,
                      asio::placeholders::bytes_transferred));
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
      std::cout << "recv: " << read_buffer_.data() << std::endl;
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

  void on_send_header(const boost::system::error_code& error, size_t bytes)
  {
    if (error)
    {
      std::cerr << "error[send header]: " << error.message() << std::endl;
      write_callback_(false);
    }
    else
    {
      asio::async_write(
          socket_, asio::buffer(write_buffer_.data(), write_buffer_.size()),
          boost::bind(&ConnectionBase::on_send, this, asio::placeholders::error,
                      asio::placeholders::bytes_transferred));
    }
  }
  void on_send(const boost::system::error_code& error, size_t bytes)
  {
    if (error)
    {
      std::cerr << "error[send body]: " << error.message() << std::endl;
      write_callback_(false);
    }
    else
    {
      write_callback_(true);
    }
  }
};

} // namespace Network
