#ifndef RMQADMIN_HTTP_H
#define RMQADMIN_HTTP_H

#include "core.h"

namespace rmqadmin {

class HttpClient {
  public:
    explicit HttpClient(const AdminConfig& config);
    Response perform(const Command& cmd);

  private:
    AdminConfig d_config;
};

}  // namespace rmqadmin

#endif  // RMQADMIN_HTTP_H
