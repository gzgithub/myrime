//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-08-08 GONG Chen <chen.sst@gmail.com>
//
#include <boost/bind.hpp>
#include <rime/context.h>
#include <rime/engine.h>
#include <rime/schema.h>
#include <rime/service.h>
#include <rime/switcher.h>

namespace rime {

Session::Session() : last_active_time_(0) {
  switcher_.reset(new Switcher);
  engine_.reset(Engine::Create(switcher_->CreateSchema()));
  switcher_->Attach(engine_.get());
  engine_->Attach(switcher_.get());
  engine_->sink().connect(boost::bind(&Session::OnCommit, this, _1));
  SessionId session_id = reinterpret_cast<SessionId>(this);
  engine_->message_sink().connect(
      boost::bind(&Service::Notify, &Service::instance(), session_id, _1, _2));
}

bool Session::ProcessKeyEvent(const KeyEvent &key_event) {
  return switcher_->ProcessKeyEvent(key_event) ||
      engine_->ProcessKeyEvent(key_event);
}

void Session::Activate() {
  last_active_time_ = time(NULL);
}

void Session::ResetCommitText() {
  commit_text_.clear();
}

bool Session::CommitComposition() {
  if (!engine_)
    return false;
  engine_->context()->Commit();
  return !commit_text_.empty();
}

void Session::ClearComposition() {
  if (!engine_)
    return;
  engine_->context()->Clear();
}

void Session::ApplySchema(Schema* schema) {
  switcher_->ApplySchema(schema);
}

void Session::OnCommit(const std::string &commit_text) {
  commit_text_ += commit_text;
}

Context* Session::context() const {
  if (switcher_->active()) {
    return switcher_->context();
  }
  return engine_ ? engine_->context() : NULL;
}

Schema* Session::schema() const {
  if (switcher_->active()) {
    return switcher_->schema();
  }
  return engine_ ? engine_->schema() : NULL;
}

Service::Service() : started_(false) {
  deployer_.message_sink().connect(
      boost::bind(&Service::Notify, this, 0, _1, _2));
}

Service::~Service() {
  StopService();
}

void Service::StartService() {
  started_ = true;
}

void Service::StopService() {
  started_ = false;
  CleanupAllSessions();
}

SessionId Service::CreateSession() {
  SessionId id = kInvalidSessionId;
  if (disabled()) return id;
  try {
    shared_ptr<Session> session = make_shared<Session>();
    session->Activate();
    id = reinterpret_cast<uintptr_t>(session.get());
    sessions_[id] = session;
  }
  catch (const std::exception& ex) {
    LOG(ERROR) << "Error creating session: " << ex.what();
  }
  catch (const std::string& ex) {
    LOG(ERROR) << "Error creating session: " << ex;
  }
  catch (const char* ex) {
    LOG(ERROR) << "Error creating session: " << ex;
  }
  catch (int ex) {
    LOG(ERROR) << "Error creating session: " << ex;
  }
  catch (...) {
    LOG(ERROR) << "Error creating session.";
  }
  return id;
}

shared_ptr<Session> Service::GetSession(SessionId session_id) {
  shared_ptr<Session> session;
  if (disabled()) return session;
  SessionMap::iterator it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    session = it->second;
    session->Activate();
  }
  return session;
}

bool Service::DestroySession(SessionId session_id) {
  SessionMap::iterator it = sessions_.find(session_id);
  if (it == sessions_.end())
    return false;
  sessions_.erase(it);
  return true;
}

void Service::CleanupStaleSessions() {
  time_t now = time(NULL);
  int count = 0;
  for (SessionMap::iterator it = sessions_.begin();
       it != sessions_.end(); ) {
    if (it->second &&
        it->second->last_active_time() < now - Session::kLifeSpan) {
      sessions_.erase(it++);
      ++count;
    }
    else
      ++it;
  }
  if (count > 0) {
    LOG(INFO) << "Recycled " << count << " stale sessions.";
  }
}

void Service::CleanupAllSessions() {
  sessions_.clear();
}

void Service::SetNotificationHandler(const NotificationHandler& handler) {
  notification_handler_ = handler;
}

void Service::ClearNotificationHandler() {
  notification_handler_.clear();
}

void Service::Notify(SessionId session_id,
                     const std::string& message_type,
                     const std::string& message_value) {
  if (notification_handler_) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    notification_handler_(session_id,
                          message_type.c_str(), message_value.c_str());
  }
}

Service& Service::instance() {
  static scoped_ptr<Service> s_instance;
  if (!s_instance) {
    s_instance.reset(new Service);
  }
  return *s_instance;
}

}  // namespace rime
