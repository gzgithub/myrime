//
// Copyleft RIME Developers
// License: GPLv3
//
// 2013-07-02 GONG Chen <chen.sst@gmail.com>
//

#ifndef RIME_FORMATTER_H_
#define RIME_FORMATTER_H_

#include <rime/common.h>
#include <rime/component.h>
#include <rime/ticket.h>

namespace rime {

class Engine;

class Formatter : public Class<Formatter, const Ticket&> {
 public:
  Formatter(const Ticket& ticket)
      : engine_(ticket.engine), name_space_(ticket.name_space) {}
  virtual ~Formatter() {}

  virtual void Format(std::string* text) = 0;

 protected:
  Engine *engine_;
  std::string name_space_;
};

}  // namespace rime

#endif  // RIME_FORMATTER_H_
