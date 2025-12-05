#ifndef RMQADMIN_HTTP_CLIENT_H
#define RMQADMIN_HTTP_CLIENT_H

#include "core.h"

#include <boost/asio/io_context.hpp>

namespace rmqadmin {

class HttpClient {
  public:
    HttpClient(const AdminConfig& config, boost::asio::io_context& io);
    Response perform(const Command& cmd);

  private:
    AdminConfig d_config;
    boost::asio::io_context& d_io;
};

}  // namespace rmqadmin

#endif  // RMQADMIN_HTTP_CLIENT_H
