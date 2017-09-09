//
// Copyleft RIME Developers
// License: GPLv3
//
// 2013-07-17 GONG Chen <chen.sst@gmail.com>
//
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <utf8.h>
#include <rime/config.h>
#include <rime/algo/encoder.h>

namespace rime {

std::string RawCode::ToString() const {
  return boost::join(*this, " ");
}

void RawCode::FromString(const std::string &code_str) {
  boost::split(*dynamic_cast<std::vector<std::string> *>(this),
               code_str,
               boost::algorithm::is_space(),
               boost::algorithm::token_compress_on);
}

TableEncoder::TableEncoder(PhraseCollector* collector)
    : Encoder(collector), loaded_(false) {
}

/*
  # sample encoder configuration (from cangjie5.dict.yaml)
  encoder:
  exclude_patterns:
  - '^x.*$'
  - '^z.*$'
  rules:
  - length_equal: 2
  formula: "AaAzBaBbBz"
  - length_equal: 3
  formula: "AaAzBaBzCz"
  - length_in_range: [4, 10]
  formula: "AaBzCaYzZz"
  tail_anchor: "'"
*/
bool TableEncoder::LoadSettings(Config* config) {
  loaded_ = false;
  encoding_rules_.clear();
  exclude_patterns_.clear();
  tail_anchor_.clear();

  if (!config) return false;

  if (ConfigListPtr rules = config->GetList("encoder/rules")) {
    for (ConfigList::Iterator it = rules->begin(); it != rules->end(); ++it) {
      ConfigMapPtr rule = As<ConfigMap>(*it);
      if (!rule || !rule->HasKey("formula"))
        continue;
      const std::string formula(rule->GetValue("formula")->str());
      TableEncodingRule r;
      if (!ParseFormula(formula, &r))
        continue;
      r.min_word_length = r.max_word_length = 0;
      if (ConfigValuePtr value = rule->GetValue("length_equal")) {
        int length = 0;
        if (!value->GetInt(&length)) {
          LOG(ERROR) << "invalid length";
          continue;
        }
        r.min_word_length = r.max_word_length = length;
      }
      else if (ConfigListPtr range =
               As<ConfigList>(rule->Get("length_in_range"))) {
        if (range->size() != 2 ||
            !range->GetValueAt(0) ||
            !range->GetValueAt(1) ||
            !range->GetValueAt(0)->GetInt(&r.min_word_length) ||
            !range->GetValueAt(1)->GetInt(&r.max_word_length) ||
            r.min_word_length > r.max_word_length) {
          LOG(ERROR) << "invalid range.";
          continue;
        }
      }
      encoding_rules_.push_back(r);
    }
  }
  if (ConfigListPtr excludes = config->GetList("encoder/exclude_patterns")) {
    for (ConfigList::Iterator it = excludes->begin();
         it != excludes->end(); ++it) {
      ConfigValuePtr pattern = As<ConfigValue>(*it);
      if (!pattern)
        continue;
      exclude_patterns_.push_back(boost::regex(pattern->str()));
    }
  }
  config->GetString("encoder/tail_anchor", &tail_anchor_);

  loaded_ = !encoding_rules_.empty();
  return loaded_;
}

bool TableEncoder::ParseFormula(const std::string& formula,
                                TableEncodingRule* rule) {
  if (formula.length() % 2 != 0) {
    LOG(ERROR) << "bad formula: '%s'" << formula;
    return false;
  }
  for (std::string::const_iterator it = formula.begin(), end = formula.end();
       it != end; ) {
    CodeCoords c;
    if (*it < 'A' || *it > 'Z') {
      LOG(ERROR) << "invalid character index in formula: '%s'" << formula;
      return false;
    }
    c.char_index = (*it >= 'U') ? (*it - 'Z' - 1) : (*it - 'A');
    ++it;
    if (*it < 'a' || *it > 'z') {
      LOG(ERROR) << "invalid code index in formula: '%s'" << formula;
      return false;
    }
    c.code_index = (*it >= 'u') ? (*it - 'z' - 1) : (*it - 'a');
    ++it;
    rule->coords.push_back(c);
  }
  return true;
}

bool TableEncoder::IsCodeExcluded(const std::string& code) {
  BOOST_FOREACH(const boost::regex& pattern, exclude_patterns_) {
    if (boost::regex_match(code, pattern))
      return true;
  }
  return false;
}

bool TableEncoder::Encode(const RawCode& code, std::string* result) {
  int num_syllables = static_cast<int>(code.size());
  BOOST_FOREACH(const TableEncodingRule& rule, encoding_rules_) {
    if (num_syllables < rule.min_word_length ||
        num_syllables > rule.max_word_length) {
      continue;
    }
    result->clear();
    CodeCoords previous = {0, 0};
    CodeCoords encoded = {0, 0};
    BOOST_FOREACH(const CodeCoords& current, rule.coords) {
      CodeCoords c(current);
      if (c.char_index < 0) {
        c.char_index += num_syllables;
      }
      if (c.char_index >= num_syllables) {
        continue;  // 'abc def' ~ 'Ca'
      }
      if (c.char_index < 0) {
        continue;  // 'abc def' ~ 'Xa'
      }
      if (current.char_index < 0 &&
          c.char_index < encoded.char_index) {
        continue;  // 'abc def' ~ '(AaBa)Ya'
        // 'abc def' ~ '(AaBa)Aa' is OK
      }
      int start_index = 0;
      if (c.char_index == encoded.char_index) {
        start_index = encoded.code_index + 1;
      }
      c.code_index = CalculateCodeIndex(code[c.char_index],  c.code_index,
                                        start_index);
      if (c.code_index >= static_cast<int>(code[c.char_index].length())) {
        continue;  // 'abc def' ~ 'Ad'
      }
      if (c.code_index < 0) {
        continue;  // 'abc def' ~ 'Ax'
      }
      if ((current.char_index < 0 || current.code_index < 0) &&
          c.char_index == encoded.char_index &&
          c.code_index <= encoded.code_index &&
          (current.char_index != previous.char_index ||
           current.code_index != previous.code_index)) {
        continue;  // 'abc def' ~ '(AaBb)By', '(AaBb)Zb', '(AaZb)Zy'
        // 'abc def' ~ '(AaZb)Zb' is OK
        // 'abc def' ~ '(AaZb)Zz' is OK
      }
      *result += code[c.char_index][c.code_index];
      previous = current;
      encoded = c;
    }
    if (result->empty()) {
      continue;
    }
    return true;
  }

  return false;
}

// index: 0-based virtual index of encoding characters in `code`.
//        counting from the end of `code` if `index` is negative.
//        tail anchors do not count as encoding characters.
// start: when `index` is negative, the first appearance of a tail anchor
//        beyond `start` is used to locate the encoding character at index -1.
// returns string index in `code` for the character at virtual `index`.
// may return a negative number if `index` does not exist in `code`.
int TableEncoder::CalculateCodeIndex(const std::string& code, int index,
                                     int start) {
  DLOG(INFO) << "code = " << code
             << ", index = " << index << ", start = " << start;
  // tail_anchor = '|'
  const int n = static_cast<int>(code.length());
  int k = 0;
  if (index < 0) {
    // 'ab|cd|ef|g' ~ '(Aa)Az' -> 'ab'; start = 1, index = -1
    // 'ab|cd|ef|g' ~ '(AaAb)Az' -> 'abd'; start = 4, index = -1
    // 'ab|cd|ef|g' ~ '(AaAb)Ay' -> 'abc'; start = 4, index = -2
    k = n - 1;
    size_t tail = code.find_first_of(tail_anchor_, start + 1);
    if (tail != std::string::npos) {
      k = static_cast<int>(tail) - 1;
    }
    while (++index < 0) {
      while (--k >= 0 &&
             tail_anchor_.find(code[k]) != std::string::npos) {
      }
    }
  }
  else {
    // 'ab|cd|ef|g' ~ '(AaAb)Ac' -> 'abc'; index = 2
    while (index-- > 0) {
      while (++k < n &&
             tail_anchor_.find(code[k]) != std::string::npos) {
      }
    }
  }
  return k;
}

bool TableEncoder::EncodePhrase(const std::string& phrase,
                                const std::string& value) {
  RawCode code;
  return DfsEncode(phrase, value, 0, &code);
}

bool TableEncoder::DfsEncode(const std::string& phrase,
                             const std::string& value,
                             size_t start_pos,
                             RawCode* code) {
  if (start_pos == phrase.length()) {
    std::string encoded;
    if (Encode(*code, &encoded)) {
      DLOG(INFO) << "encode '" << phrase << "': "
                 << "[" << code->ToString() << "] -> [" << encoded << "]";
      collector_->CreateEntry(phrase, encoded, value);
      return true;
    }
    else {
      LOG(WARNING) << "failed to encode '" << phrase << "': "
                   << "[" << code->ToString() << "]";
      return false;
    }
  }
  const char* word_start = phrase.c_str() + start_pos;
  const char* word_end = word_start;
  utf8::unchecked::next(word_end);
  size_t word_len = word_end - word_start;
  std::string word(word_start, word_len);
  bool ret = false;
  std::vector<std::string> translations;
  if (collector_->TranslateWord(word, &translations)) {
    BOOST_FOREACH(const std::string& x, translations) {
      if (IsCodeExcluded(x)) {
        continue;
      }
      code->push_back(x);
      bool ok = DfsEncode(phrase, value, start_pos + word_len, code);
      ret = ret || ok;
      code->pop_back();
    }
  }
  return ret;
}

ScriptEncoder::ScriptEncoder(PhraseCollector* collector)
    : Encoder(collector) {
}

bool ScriptEncoder::EncodePhrase(const std::string& phrase,
                                 const std::string& value) {
  RawCode code;
  return DfsEncode(phrase, value, 0, &code);
}

bool ScriptEncoder::DfsEncode(const std::string& phrase,
                              const std::string& value,
                              size_t start_pos,
                              RawCode* code) {
  if (start_pos == phrase.length()) {
    collector_->CreateEntry(phrase, code->ToString(), value);
    return true;
  }
  bool ret = false;
  for (size_t k = phrase.length() - start_pos; k > 0; --k) {
    std::string word(phrase.substr(start_pos, k));
    std::vector<std::string> translations;
    if (collector_->TranslateWord(word, &translations)) {
      BOOST_FOREACH(const std::string& x, translations) {
        code->push_back(x);
        bool ok = DfsEncode(phrase, value, start_pos + k, code);
        ret = ret || ok;
        code->pop_back();
      }
    }
  }
  return ret;
}

}  // namespace rime
