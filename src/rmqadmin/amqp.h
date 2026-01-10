#ifndef RMQADMIN_AMQP_H
#define RMQADMIN_AMQP_H

#include "core.h"

namespace rmqadmin {

class AmqpClient {
  public:
    AmqpClient(const AdminConfig& config, void* logger);
    Response perform(const Command& cmd);

  private:
    AdminConfig d_config;
    void* d_logger;  // quill::Logger*
};

}  // namespace rmqadmin

#endif  // RMQADMIN_AMQP_H
