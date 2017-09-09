//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-05-08 GONG Chen <chen.sst@gmail.com>
//
#include <rime/candidate.h>
#include <rime/context.h>
#include <rime/composition.h>
#include <rime/menu.h>
#include <rime/segmentation.h>

namespace rime {

Context::Context() : input_(), caret_pos_(0), composition_(new Composition) {
}

Context::~Context() {
}

bool Context::Commit() {
  if (!IsComposing())
    return false;
  // notify the engine and interesting components
  commit_notifier_(this);
  // start over
  Clear();
  return true;
}

std::string Context::GetCommitText() const {
  if (get_option("dumb"))
    return std::string();
  return composition_->GetCommitText();
}

std::string Context::GetScriptText() const {
  return composition_->GetScriptText();
}

void Context::GetPreedit(Preedit *preedit, bool soft_cursor) const {
  composition_->GetPreedit(preedit);
  preedit->caret_pos = preedit->text.length();
  if (IsComposing()) {
    if (soft_cursor) {
      const std::string caret("\xe2\x80\xba");
      preedit->text.append(caret);
    }
    if (caret_pos_ < input_.length()) {
      preedit->text.append(input_.substr(caret_pos_));
    }
  }
}

bool Context::IsComposing() const {
  return !input_.empty();
}

bool Context::HasMenu() const {
  if (composition_->empty())
    return false;
  const shared_ptr<Menu>& menu(composition_->back().menu);
  return menu && !menu->empty();
}

bool Context::PushInput(char ch) {
  if (caret_pos_ >= input_.length()) {
    input_.push_back(ch);
    caret_pos_ = input_.length();
  }
  else {
    input_.insert(caret_pos_, 1, ch);
    ++caret_pos_;
  }
  update_notifier_(this);
  return true;
}

bool Context::PushInput(const std::string& str) {
  if (caret_pos_ >= input_.length()) {
    input_ += str;
    caret_pos_ = input_.length();
  }
  else {
    input_.insert(caret_pos_, str);
    caret_pos_ += str.length();
  }
  update_notifier_(this);
  return true;
}

bool Context::PopInput(size_t len) {
  if (caret_pos_ < len)
    return false;
  caret_pos_ -= len;
  input_.erase(caret_pos_, len);
  update_notifier_(this);
  return true;
}

bool Context::DeleteInput(size_t len) {
  if (caret_pos_ + len > input_.length())
    return false;
  input_.erase(caret_pos_, len);
  update_notifier_(this);
  return true;
}

void Context::Clear() {
  input_.clear();
  caret_pos_ = 0;
  composition_->clear();
  update_notifier_(this);
}

bool Context::Select(size_t index) {
  if (composition_->empty())
    return false;
  Segment &seg(composition_->back());
  shared_ptr<Candidate> cand(seg.GetCandidateAt(index));
  if (cand) {
    seg.selected_index = index;
    seg.status = Segment::kSelected;
    DLOG(INFO) << "Selected: '" << cand->text() << "', index = " << index;
    select_notifier_(this);
    return true;
  }
  return false;
}

bool Context::DeleteCurrentSelection() {
  if (composition_->empty())
    return false;
  Segment &seg(composition_->back());
  shared_ptr<Candidate> cand(seg.GetSelectedCandidate());
  if (cand) {
    DLOG(INFO) << "Deleting: '" << cand->text()
               << "', selected_index = " << seg.selected_index;
    delete_notifier_(this);
    return true;  // CAVEAT: this doesn't mean anything is deleted for sure
  }
  return false;
}

bool Context::ConfirmCurrentSelection() {
  if (composition_->empty())
    return false;
  Segment &seg(composition_->back());
  seg.status = Segment::kSelected;
  shared_ptr<Candidate> cand(seg.GetSelectedCandidate());
  if (cand) {
    DLOG(INFO) << "Confirmed: '" << cand->text()
               << "', selected_index = " << seg.selected_index;
  }
  else {
    if (seg.end == seg.start) {
      // fluency editor will confirm the whole sentence
      return false;
    }
    // confirm raw input
  }
  select_notifier_(this);
  return true;
}

bool Context::ConfirmPreviousSelection() {
  for (Composition::reverse_iterator it = composition_->rbegin();
       it != composition_->rend(); ++it) {
    if (it->status > Segment::kSelected) return false;
    if (it->status == Segment::kSelected) {
      it->status = Segment::kConfirmed;
      return true;
    }
  }
  return false;
}

bool Context::ReopenPreviousSegment() {
  if (composition_->Trim()) {
    if (!composition_->empty() &&
      composition_->back().status >= Segment::kSelected) {
      composition_->back().status = Segment::kVoid;
    }
    update_notifier_(this);
    return true;
  }
  return false;
}

bool Context::ClearPreviousSegment() {
  if (composition_->empty())
    return false;
  size_t where = composition_->back().start;
  if (where >= input_.length())
    return false;
  set_input(input_.substr(0, where));
  return true;
}

bool Context::ReopenPreviousSelection() {
  for (Composition::reverse_iterator it = composition_->rbegin();
       it != composition_->rend(); ++it) {
    if (it->status > Segment::kSelected) return false;
    if (it->status == Segment::kSelected) {
      it->status = Segment::kVoid;
      while (it != composition_->rbegin())
        composition_->pop_back();
      update_notifier_(this);
      return true;
    }
  }
  return false;
}

bool Context::ClearNonConfirmedComposition() {
  bool reverted = false;
  while (!composition_->empty() &&
         composition_->back().status < Segment::kSelected) {
    composition_->pop_back();
    reverted = true;
  }
  if (reverted) {
    composition_->Forward();
    DLOG(INFO) << "composition: " << composition_->GetDebugText();
  }
  return reverted;
}

bool Context::RefreshNonConfirmedComposition() {
  if (ClearNonConfirmedComposition()) {
    update_notifier_(this);
    return true;
  }
  return false;
}

void Context::set_caret_pos(size_t caret_pos) {
  if (caret_pos > input_.length())
    caret_pos_ = input_.length();
  else
    caret_pos_ = caret_pos;
  update_notifier_(this);
}

void Context::set_composition(Composition *comp) {
  if (composition_.get() != comp)
    composition_.reset(comp);
  // TODO: notification
}

void Context::set_input(const std::string &value) {
  input_ = value;
  caret_pos_ = input_.length();
  update_notifier_(this);
}

Composition* Context::composition() {
  return composition_.get();
}

const Composition* Context::composition() const {
  return composition_.get();
}

void Context::set_option(const std::string &name, bool value) {
  options_[name] = value;
  option_update_notifier_(this, name);
}

bool Context::get_option(const std::string &name) const {
  std::map<std::string, bool>::const_iterator it = options_.find(name);
  if (it != options_.end())
    return it->second;
  else
    return false;
}

void Context::set_property(const std::string &name,
                           const std::string& value) {
  properties_[name] = value;
}

std::string Context::get_property(const std::string &name) const {
  std::map<std::string, std::string>::const_iterator it =
      properties_.find(name);
  if (it != properties_.end())
    return it->second;
  else
    return std::string();
}

void Context::ClearTransientOptions() {
  std::map<std::string, bool>::iterator oit = options_.lower_bound("_");
  while (oit != options_.end() &&
         !oit->first.empty() && oit->first[0] == '_') {
    options_.erase(oit++);
  }
  std::map<std::string, std::string>::iterator pit =
      properties_.lower_bound("_");
  while (pit != properties_.end() &&
         !pit->first.empty() && pit->first[0] == '_') {
    properties_.erase(pit++);
  }
}

}  // namespace rime
