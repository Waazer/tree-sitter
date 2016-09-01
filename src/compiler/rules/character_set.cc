#include "compiler/rules/character_set.h"
#include <string>
#include <utility>
#include <vector>
#include "compiler/rules/visitor.h"

namespace tree_sitter {
namespace rules {

using std::string;
using std::hash;
using std::set;
using std::vector;

static void add_range(set<uint32_t> *characters, uint32_t min, uint32_t max) {
  for (uint32_t c = min; c <= max; c++)
    characters->insert(c);
}

static void remove_range(set<uint32_t> *characters, uint32_t min, uint32_t max) {
  for (uint32_t c = min; c <= max; c++)
    characters->erase(c);
}

static set<uint32_t> remove_chars(set<uint32_t> *left,
                                  const set<uint32_t> &right) {
  set<uint32_t> result;
  for (uint32_t c : right) {
    if (left->erase(c))
      result.insert(c);
  }
  return result;
}

static set<uint32_t> add_chars(set<uint32_t> *left, const set<uint32_t> &right) {
  set<uint32_t> result;
  for (uint32_t c : right)
    if (left->insert(c).second)
      result.insert(c);
  return result;
}

static vector<CharacterRange> consolidate_ranges(const set<uint32_t> &chars) {
  vector<CharacterRange> result;
  for (uint32_t c : chars) {
    size_t size = result.size();
    if (size >= 2 && result[size - 2].max == (c - 2)) {
      result.pop_back();
      result.back().max = c;
    } else if (size >= 1) {
      CharacterRange &last = result.back();
      if (last.min < last.max && last.max == (c - 1))
        last.max = c;
      else
        result.push_back(CharacterRange(c));
    } else {
      result.push_back(CharacterRange(c));
    }
  }
  return result;
}

CharacterSet::CharacterSet()
    : includes_all(false), included_chars({}), excluded_chars({}) {}

bool CharacterSet::operator==(const Rule &rule) const {
  const CharacterSet *other = rule.as<CharacterSet>();
  return other && (includes_all == other->includes_all) &&
         (included_chars == other->included_chars) &&
         (excluded_chars == other->excluded_chars);
}

bool CharacterSet::operator<(const CharacterSet &other) const {
  if (!includes_all && other.includes_all)
    return true;
  if (includes_all && !other.includes_all)
    return false;
  if (included_chars < other.included_chars)
    return true;
  if (other.included_chars < included_chars)
    return false;
  return excluded_chars < other.excluded_chars;
}

size_t CharacterSet::hash_code() const {
  size_t result = hash<bool>()(includes_all);
  result ^= hash<size_t>()(included_chars.size());
  for (auto &c : included_chars)
    result ^= hash<uint32_t>()(c);
  result <<= 1;
  result ^= hash<size_t>()(excluded_chars.size());
  for (auto &c : excluded_chars)
    result ^= hash<uint32_t>()(c);
  return result;
}

rule_ptr CharacterSet::copy() const {
  return std::make_shared<CharacterSet>(*this);
}

string CharacterSet::to_string() const {
  string result("(char");
  if (includes_all)
    result += " include_all";
  if (!included_chars.empty()) {
    result += " (include";
    for (auto r : included_ranges())
      result += string(" ") + r.to_string();
    result += ")";
  }
  if (!excluded_chars.empty()) {
    result += " (exclude";
    for (auto r : excluded_ranges())
      result += string(" ") + r.to_string();
    result += ")";
  }
  return result + ")";
}

CharacterSet &CharacterSet::include_all() {
  includes_all = true;
  included_chars = {};
  excluded_chars = { 0 };
  return *this;
}

CharacterSet &CharacterSet::include(uint32_t min, uint32_t max) {
  if (includes_all)
    remove_range(&excluded_chars, min, max);
  else
    add_range(&included_chars, min, max);
  return *this;
}

CharacterSet &CharacterSet::exclude(uint32_t min, uint32_t max) {
  if (includes_all)
    add_range(&excluded_chars, min, max);
  else
    remove_range(&included_chars, min, max);
  return *this;
}

CharacterSet &CharacterSet::include(uint32_t c) {
  return include(c, c);
}

CharacterSet &CharacterSet::exclude(uint32_t c) {
  return exclude(c, c);
}

bool CharacterSet::is_empty() const {
  return !includes_all && included_chars.empty();
}

void CharacterSet::add_set(const CharacterSet &other) {
  if (includes_all) {
    if (other.includes_all) {
      excluded_chars = remove_chars(&excluded_chars, other.excluded_chars);
    } else {
      remove_chars(&excluded_chars, other.included_chars);
    }
  } else {
    if (other.includes_all) {
      includes_all = true;
      for (uint32_t c : other.excluded_chars)
        if (!included_chars.count(c))
          excluded_chars.insert(c);
      included_chars.clear();
    } else {
      for (uint32_t c : other.included_chars)
        included_chars.insert(c);
    }
  }
}

CharacterSet CharacterSet::remove_set(const CharacterSet &other) {
  CharacterSet result;
  if (includes_all) {
    if (other.includes_all) {
      result.includes_all = true;
      result.excluded_chars = excluded_chars;
      included_chars = add_chars(&result.excluded_chars, other.excluded_chars);
      excluded_chars = {};
      includes_all = false;
    } else {
      result.included_chars = add_chars(&excluded_chars, other.included_chars);
    }
  } else {
    if (other.includes_all) {
      result.included_chars = included_chars;
      included_chars =
        remove_chars(&result.included_chars, other.excluded_chars);
    } else {
      result.included_chars =
        remove_chars(&included_chars, other.included_chars);
    }
  }
  return result;
}

bool CharacterSet::intersects(const CharacterSet &other) const {
  CharacterSet copy(*this);
  return !copy.remove_set(other).is_empty();
}

vector<CharacterRange> CharacterSet::included_ranges() const {
  return consolidate_ranges(included_chars);
}

vector<CharacterRange> CharacterSet::excluded_ranges() const {
  return consolidate_ranges(excluded_chars);
}

void CharacterSet::accept(Visitor *visitor) const {
  visitor->visit(this);
}

}  // namespace rules
}  // namespace tree_sitter
