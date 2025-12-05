#ifndef RMQADMIN_AMQP_H
#define RMQADMIN_AMQP_H

#include "core.h"

namespace rmqadmin {

class AmqpClient {
  public:
    explicit AmqpClient(const AdminConfig& config);
    Response perform(const Command& cmd);

  private:
    AdminConfig d_config;
};

}  // namespace rmqadmin

#endif  // RMQADMIN_AMQP_H
