#ifndef RMQADMIN_EXEC_H
#define RMQADMIN_EXEC_H

#include "core.h"

namespace rmqadmin {

class Executor {
  public:
    explicit Executor(const AdminConfig& cfg);
    Response run(const Command& cmd);

  private:
    AdminConfig d_cfg;
};

}  // namespace rmqadmin

#endif  // RMQADMIN_EXEC_H
