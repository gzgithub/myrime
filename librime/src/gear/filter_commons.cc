//
// Copyleft RIME Developers
// License: GPLv3
//
// 2013-11-05 GONG Chen <chen.sst@gmail.com>
//
#include <boost/foreach.hpp>
#include <rime/schema.h>
#include <rime/segmentation.h>
#include <rime/ticket.h>
#include <rime/gear/filter_commons.h>

namespace rime {

TagMatching::TagMatching(const Ticket& ticket) {
  if (!ticket.schema) return;
  Config* config = ticket.schema->config();
  if (ConfigListPtr tags = config->GetList(ticket.name_space + "/tags")) {
    for (ConfigList::Iterator it = tags->begin(); it != tags->end(); ++it) {
      if (Is<ConfigValue>(*it)) {
        tags_.push_back(As<ConfigValue>(*it)->str());
      }
    }
  }
}

bool TagMatching::TagsMatch(Segment* segment) {
  if (!segment)
    return false;
  if (tags_.empty())  // match any
    return true;
  BOOST_FOREACH(const std::string& tag, tags_) {
    if (segment->HasTag(tag))
      return true;
  }
  return false;
}

}  // namespace rime
