//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-12-07 GONG Chen <chen.sst@gmail.com>
//
#include <string>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <rime/candidate.h>
#include <rime/common.h>
#include <rime/composition.h>
#include <rime/config.h>
#include <rime/context.h>
#include <rime/key_event.h>
#include <rime/menu.h>
#include <rime/processor.h>
#include <rime/schema.h>
#include <rime/switcher.h>
#include <rime/ticket.h>
#include <rime/translation.h>
#include <rime/translator.h>

namespace rime {

Switcher::Switcher() : Engine(new Schema),
                       active_(false) {
  context_->set_option("dumb", true);  // not going to commit anything

  // receive context notifications
  context_->select_notifier().connect(
      boost::bind(&Switcher::OnSelect, this, _1));

  user_config_.reset(Config::Require("config")->Create("user"));
  InitializeComponents();
  LoadSettings();
}

Switcher::~Switcher() {
}

void Switcher::Attach(Engine *engine) {
  attached_engine_ = engine;
  // restore saved options
  if (user_config_) {
    BOOST_FOREACH(const std::string& option_name, save_options_) {
      bool value = false;
      if (user_config_->GetBool("var/option/" + option_name, &value)) {
        engine->context()->set_option(option_name, value);
      }
    }
  }
}

bool Switcher::ProcessKeyEvent(const KeyEvent &key_event) {
  BOOST_FOREACH(const KeyEvent &hotkey, hotkeys_) {
    if (key_event == hotkey) {
      if (!active_ && attached_engine_) {
        Activate();
      }
      else if (active_) {
        HighlightNextSchema();
      }
      return true;
    }
  }
  if (active_) {
    BOOST_FOREACH(shared_ptr<Processor> &p, processors_) {
      if (kNoop != p->ProcessKeyEvent(key_event))
        return true;
    }
    if (key_event.release() || key_event.ctrl() || key_event.alt())
      return true;
    int ch = key_event.keycode();
    if (ch == XK_space || ch == XK_Return) {
      context_->ConfirmCurrentSelection();
    }
    else if (ch == XK_Escape) {
      Deactivate();
    }
    return true;
  }
  return false;
}

void Switcher::HighlightNextSchema() {
  Composition *comp = context_->composition();
  if (!comp || comp->empty() || !comp->back().menu)
    return;
  Segment& seg(comp->back());
  int index = seg.selected_index;
  shared_ptr<Candidate> option;
  do {
    ++index;  // next
    int candidate_count = seg.menu->Prepare(index + 1);
    if (candidate_count <= index) {
      index = 0;  // passed the end; rewind
      break;
    }
    else {
      option = seg.GetCandidateAt(index);
    }
  }
  while (!option || option->type() != "schema");
  seg.selected_index = index;
  seg.tags.insert("paging");
  return;
}

Schema* Switcher::CreateSchema() {
  Config *config = schema_->config();
  if (!config) return NULL;
  ConfigListPtr schema_list = config->GetList("schema_list");
  if (!schema_list) return NULL;
  std::string previous;
  if (user_config_) {
    user_config_->GetString("var/previously_selected_schema", &previous);
  }
  std::string recent;
  for (size_t i = 0; i < schema_list->size(); ++i) {
    ConfigMapPtr item = As<ConfigMap>(schema_list->GetAt(i));
    if (!item) continue;
    ConfigValuePtr schema_property = item->GetValue("schema");
    if (!schema_property) continue;
    const std::string &schema_id(schema_property->str());
    if (previous.empty() || previous == schema_id) {
      recent = schema_id;
      break;
    }
    if (recent.empty())
      recent = schema_id;
  }
  if (recent.empty())
    return NULL;
  else
    return new Schema(recent);
}

void Switcher::ApplySchema(Schema* schema) {
  if (!schema) return;
  if (active_) {
    Deactivate();
  }
  attached_engine_->ApplySchema(schema);
}

void Switcher::SelectNextSchema() {
  if (translators_.empty()) return;
  shared_ptr<Translator> x = translators_[0];  // schema_list_translator
  if (!x) return;
  Menu menu;
  menu.AddTranslation(x->Query("", Segment(), NULL));
  if (menu.Prepare(2) < 2) return;
  shared_ptr<SwitcherCommand> command =
      As<SwitcherCommand>(menu.GetCandidateAt(1));
  if (!command) return;
  command->Apply(this);
}

bool Switcher::IsAutoSave(const std::string& option) const {
  return save_options_.find(option) != save_options_.end();
}

void Switcher::OnSelect(Context *ctx) {
  LOG(INFO) << "a switcher option is selected.";
  Segment &seg(ctx->composition()->back());
  shared_ptr<SwitcherCommand> command =
      As<SwitcherCommand>(seg.GetSelectedCandidate());
  if (!command) return;
  if (attached_engine_) {
    command->Apply(this);
  }
  Deactivate();
}

void Switcher::Activate() {
  LOG(INFO) << "switcher is activated.";
  Composition *comp = context_->composition();
  if (comp->empty()) {
    context_->set_input(" ");  // make context_->IsComposing() == true
    Segment seg(0, 0);         // empty range
    seg.prompt = caption_;
    comp->AddSegment(seg);
  }
  shared_ptr<Menu> menu = make_shared<Menu>();
  comp->back().menu = menu;
  BOOST_FOREACH(shared_ptr<Translator>& translator, translators_) {
    shared_ptr<Translation> t = translator->Query("", comp->back(), NULL);
    if (t)
      menu->AddTranslation(t);
  }

  // activated!
  active_ = true;
}

void Switcher::Deactivate() {
  context_->Clear();
  active_ = false;
}

void Switcher::LoadSettings() {
  Config *config = schema_->config();
  if (!config) return;
  if (!config->GetString("switcher/caption", &caption_) || caption_.empty()) {
    caption_ = ":-)";
  }
  ConfigListPtr hotkeys = config->GetList("switcher/hotkeys");
  if (!hotkeys) return;
  hotkeys_.clear();
  for (size_t i = 0; i < hotkeys->size(); ++i) {
    ConfigValuePtr value = hotkeys->GetValueAt(i);
    if (!value) continue;
    hotkeys_.push_back(KeyEvent(value->str()));
  }
  ConfigListPtr options = config->GetList("switcher/save_options");
  if (!options) return;
  save_options_.clear();
  for (ConfigList::Iterator it = options->begin(); it != options->end(); ++it) {
    ConfigValuePtr option_name = As<ConfigValue>(*it);
    if (!option_name) continue;
    save_options_.insert(option_name->str());
  }
}

void Switcher::InitializeComponents() {
  processors_.clear();
  translators_.clear();
  {
    Processor::Component* c = Processor::Require("key_binder");
    if (!c) {
      LOG(WARNING) << "key_binder not available.";
    }
    else {
      shared_ptr<Processor> p(c->Create(Ticket(this)));
      processors_.push_back(p);
    }
  }
  {
    Processor::Component* c = Processor::Require("selector");
    if (!c) {
      LOG(WARNING) << "selector not available.";
    }
    else {
      shared_ptr<Processor> p(c->Create(Ticket(this)));
      processors_.push_back(p);
    }
  }
  DLOG(INFO) << "num processors: " << processors_.size();
  {
    Translator::Component* c = Translator::Require("schema_list_translator");
    if (!c) {
      LOG(WARNING) << "schema_list_translator not available.";
    }
    else {
      shared_ptr<Translator> t(c->Create(Ticket(this)));
      translators_.push_back(t);
    }
  }
  {
    Translator::Component* c = Translator::Require("switch_translator");
    if (!c) {
      LOG(WARNING) << "switch_translator not available.";
    }
    else {
      shared_ptr<Translator> t(c->Create(Ticket(this)));
      translators_.push_back(t);
    }
  }
  DLOG(INFO) << "num translators: " << translators_.size();
}

}  // namespace rime
