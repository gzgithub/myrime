//
// Copyleft 2013 RIME Developers
// License: GPLv3
//
// 2013-04-18 GONG Chen <chen.sst@gmail.com>
//
#include <rime/dict/db.h>
#include <rime/dict/db_utils.h>

namespace rime {

int Source::Dump(Sink* sink) {
  if (!sink)
    return 0;
  int num_entries = 0;
  std::string key, value;
  while (MetaGet(&key, &value)) {
    if (sink->MetaPut(key, value))
      ++num_entries;
  }
  while (Get(&key, &value)) {
    if (sink->Put(key, value))
      ++num_entries;
  }
  return num_entries;
}

DbSink::DbSink(Db* db) : db_(db) {
}

bool DbSink::MetaPut(const std::string& key, const std::string& value) {
  return db_ && db_->MetaUpdate(key, value);
}

bool DbSink::Put(const std::string& key, const std::string& value) {
  return db_ && db_->Update(key, value);
}

DbSource::DbSource(Db* db)
    : db_(db),
      metadata_(db->QueryMetadata()),
      data_(db->QueryAll()) {
}

bool DbSource::MetaGet(std::string* key, std::string* value) {
  return metadata_ && metadata_->GetNextRecord(key, value);
}

bool DbSource::Get(std::string* key, std::string* value) {
  return data_ && data_->GetNextRecord(key, value);
}

}  // namespace rime
