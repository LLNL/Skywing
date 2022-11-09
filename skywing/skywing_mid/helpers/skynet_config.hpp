/*
MIT License

Copyright (c) 2017-2020 Matthias C. M. Troffaes

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef SKYNET_UPPER_CONFIG_HPP
#define SKYNET_UPPER_CONFIG_HPP

#include "skywing_core/skywing.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <iomanip>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <type_traits>


namespace skywing::internal {
  template <typename T>
  struct is_tag 
    : std::integral_constant<
      bool,
      std::is_base_of_v<PublishTagBase, T> || 
      std::is_base_of_v<ReduceValueTagBase, T> || 
      std::is_base_of_v<ReduceGroupTagBase, T> ||
      std::is_base_of_v<PrivateTagBase, T>
    > {};

  template<class T>
  inline constexpr bool is_tag_v = is_tag<T>::value;
}

namespace skywing::config {

namespace detail {

  /* Trim the whitespace from the front of a string in place.
   * @param s The string to trim.
   * @param loc The std::locale which defines what whitespace is.
   */
  inline void ltrim(std::string & s, const std::locale & loc) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(),
                         [&loc](char ch) { return !std::isspace(ch, loc); }));
  }

  /* Trim the whitespace from the back of a string in place.
   * @param s The string to trim.
   * @param loc The std::locale which defines what whitespace is.
   */
  inline void rtrim(std::string & s, const std::locale & loc) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [&loc](char ch) { return !std::isspace(ch, loc); }).base(),
            s.end());
  }
  
  template <class UnaryPredicate>
  inline void rtrim2(std::string& s, UnaryPredicate pred) {
    s.erase(std::find_if(s.begin(), s.end(), pred), s.end());
  }
  
  inline bool replace(std::string & str, const std::string & from, const std::string & to) {
    auto changed = false;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
      changed = true;
    }
    return changed;
  }

  
  
} // namespace detail

class Format
{
public:
  // used for generating
  const char char_section_start;
  const char char_section_end;
  const char char_assign;
  const char char_comment;

  // used for parsing
  bool is_section_start(char ch) const { return ch == char_section_start; }
  bool is_section_end(char ch) const { return ch == char_section_end; }
  bool is_assign(char ch) const { return ch == char_assign; }
  bool is_comment(char ch) const { return ch == char_comment; }

  // used for interpolation
  const char char_interpol;
  const char char_interpol_start;
  const char char_interpol_sep;
  const char char_interpol_end;

  Format(char section_start, char section_end, char assign, char comment, char interpol, char interpol_start, char interpol_sep, char interpol_end)
    : char_section_start(section_start)
    , char_section_end(section_end)
    , char_assign(assign)
    , char_comment(comment)
    , char_interpol(interpol)
    , char_interpol_start(interpol_start)
    , char_interpol_sep(interpol_sep)
    , char_interpol_end(interpol_end) {}

  Format() : Format('[', ']', '=', ';', '$', '{', ':', '}') {}

  const std::string local_symbol(const std::string& name) const {
    return char_interpol + (char_interpol_start + name + char_interpol_end);
  }

  const std::string global_symbol(const std::string& sec_name, const std::string& name) const {
    return local_symbol(sec_name + char_interpol_sep + name);
  }
};


/**
 * Extraction functions convert string data to a given type
 **/
template <typename T>
inline bool extract(const std::string & value, T & dst) {
  char c;
  std::istringstream is{ value };
  T result;
  if ((is >> std::boolalpha >> result) && !(is >> c)) {
    dst = result;
    return true;
  }
  else {
    return false;
  }
}

template <>
inline bool extract(const std::string & value, std::string & dst) {
  dst = value;
  return true;
}

template<typename T>
inline bool extract_vector(const std::string & value, std::vector<T> & dst) {
  std::istringstream is{value};
  T result;
  while (!is.eof() && is >> std::boolalpha >> result) {
    dst.push_back(result);
  }
  return !dst.empty();
}

inline decltype(auto) extract_ip_and_port(const std::string &value) {
  auto split = value.rfind(':');
  std::uint16_t port = 0;
  if (split == value.npos || split == value.size()-1) {
    auto ip = value.substr(0, split);
    return std::make_tuple(ip, port);
  }
  auto ip = value.substr(0, split);
  try {
    auto port_num = std::stoul(value.substr(split + 1));
    if (port_num > std::numeric_limits<std::uint16_t>::max()) {
        std::cerr << "warning: port number '" << port_num << "' exceeds maximum and will be truncated" << std::endl;
        port = std::numeric_limits<std::uint16_t>::max();
    } else {
        port = port_num;
    }
  } catch (std::invalid_argument& e) {
    throw std::invalid_argument("invalid port number: " + value.substr(split+1));
  }
  return std::make_tuple(ip, port);
}

template<typename TagType>
struct ReduceGroupConfig {
  using GroupTag = skywing::ReduceGroupTag<TagType>;
  using ValueTag = skywing::ReduceValueTag<TagType>;
  ReduceGroupConfig(const GroupTag& reduce_group_tag, int reduce_value_tag_index, const std::vector<ValueTag>& reduce_value_tags)
  : reduce_group_tag(reduce_group_tag),
    index(reduce_value_tag_index),
    reduce_value_tags(reduce_value_tags)
  {}

  auto get_group_tag() const { return reduce_group_tag; }
  auto get_value_tag() const { return reduce_value_tags[index]; }
  auto get_value_tags() const { return reduce_value_tags; }
private:
  const int index;
  const skywing::ReduceGroupTag<TagType> reduce_group_tag;
  const std::vector<skywing::ReduceValueTag<TagType>> reduce_value_tags;
};


class Config
{
public:
  using Section = std::map<std::string, std::string>;
  using Sections = std::map<std::string, Section>;

  Sections sections;
  std::list<std::string> errors;
  std::shared_ptr<Format> format;

  static const int max_interpolation_depth = 10;

  Config(std::string filename) : format(std::make_shared<Format>())
  {
    parse(filename);
    strip_trailing_comments();
    interpolate();
  };

  template<typename DataType>
  DataType get_value(std::string sec_name, std::string key, typename std::enable_if_t<!skywing::internal::is_tag_v<DataType>>* = 0) const {
    const auto& sec = sections.at(sec_name);
    const auto it = sec.find(key);
    if (it == sec.end()) throw std::out_of_range("Key not found: " + key);
    DataType ret;
    auto success = extract(it->second, ret);
    if (success) {
      return ret;
    } else {
      throw std::exception();
    } 
  };

  template<typename TagType>
  TagType get_value(std::string sec_name, std::string key, typename std::enable_if_t<skywing::internal::is_tag_v<TagType>>* = 0) const {
    const auto& sec = sections.at(sec_name);
    const auto it = sec.find(key);
    if (it == sec.end()) throw std::out_of_range("Key not found: " + key);
    return TagType(it->second);
  };

  template<typename DataType>
  std::vector<DataType> get_vector(std::string sec_name, std::string key, typename std::enable_if_t<!skywing::internal::is_tag_v<DataType>>* = 0) const {
    const auto& sec = sections.at(sec_name);
    const auto it = sec.find(key);
    if (it == sec.end()) throw std::out_of_range("Key not found: " + key);
    std::vector<DataType> ret_vec;
    std::istringstream list(it->second);
    std::string val_str;

    while (list >> val_str) {
      DataType ret;
      auto success = extract(val_str, ret);
      if (success) {
        ret_vec.emplace_back(ret);
      } else {
        throw std::exception();
      }
    }
    return ret_vec;
  };

  template<typename TagType>
  std::vector<TagType> get_vector(std::string sec_name, std::string key, typename std::enable_if_t<skywing::internal::is_tag_v<TagType>>* = 0) {
    const auto& sec = sections.at(sec_name);
    const auto it = sec.find(key);
    if (it == sec.end()) throw std::out_of_range("Key not found: " + key);
    std::vector<TagType> ret_vec;
    std::istringstream list(it->second);
    std::string val_str;
    while (list >> val_str) {
      ret_vec.emplace_back(TagType(val_str));
    }
    return ret_vec;
  }

  decltype(auto) get_address(std::string sec_name, std::string key) const {
    const auto& sec = sections.at(sec_name);
    const auto it = sec.find(key);
    if (it == sec.end()) 
      throw std::out_of_range("Key not found in section: '" + sec_name + ": " + key + "'.");

    auto [ip, port] = extract_ip_and_port(it->second);
    return std::make_tuple(ip, port);
  }

  decltype(auto) get_addresses(std::string sec_name, std::string key) const {
    const auto& sec = sections.at(sec_name);
    const auto it = sec.find(key);
    if (it == sec.end()) 
      throw std::out_of_range("Key not found in section: '" + sec_name + ": " + key + "'.");

    std::vector<std::tuple<std::string, std::uint16_t>> addresses;
    std::istringstream list(it->second);
    
    std::string val_str;
    while (list >> val_str) {
      addresses.emplace_back(extract_ip_and_port(val_str));
    }
    return addresses;
  }

  template<typename TagType>
  ReduceGroupConfig<TagType> get_reduce_group(std::string sec_name) const {
    const auto& sec = sections.at(sec_name);

    const auto it1 = sec.find("reduce_value_tag");
    if (it1 == sec.end()) throw std::out_of_range("Missing 'reduce_value_tag' key in reduce group section " + sec_name + ".");
    const auto reduce_value_tag_name = it1->second;

    const auto it2 = sec.find("reduce_value_tags");
    if (it2 == sec.end()) throw std::out_of_range("Missing 'reduce_value_tags' key in reduce group section " + sec_name + ".");
    const auto reduce_value_tags_names = it2->second;

    skywing::ReduceGroupTag<TagType> reduce_group_tag{sec_name};
    std::vector<skywing::ReduceValueTag<TagType>> reduce_value_tags;
    std::istringstream list(reduce_value_tags_names);

    int index = 0, i = 0;
    std::string val_str;
    while (list >> val_str) {
      if (val_str == reduce_value_tag_name) {
        index = i;
      }
      reduce_value_tags.emplace_back(val_str);
      i++;
    }

    return {reduce_group_tag, index, reduce_value_tags};
  }

  void generate(std::ostream& os) const {
    for (auto const & sec : sections) {
      os << format->char_section_start << sec.first << format->char_section_end << std::endl;
      for (auto const & val : sec.second) {
        os << val.first << format->char_assign << val.second << std::endl;
      }
      os << std::endl;
    }
  }

  void parse(std::string filename) {
    std::ifstream is(filename);
    std::string line;
    std::string section;
    const std::locale loc{"C"};
    while (std::getline(is, line)) {
      detail::ltrim(line, loc);
      detail::rtrim(line, loc);
      const auto length = line.length();
      if (length > 0) {
        const auto pos = std::find_if(line.begin(), line.end(), [this](char ch) { return format->is_assign(ch); });
        const auto & front = line.front();
        if (format->is_comment(front)) {
        }
        else if (format->is_section_start(front)) {
          if (format->is_section_end(line.back()))
            section = line.substr(1, length - 2);
          else
            errors.push_back(line);
        }
        else if (pos != line.begin() && pos != line.end()) {
          std::string variable(line.begin(), pos);
          std::string value(pos + 1, line.end());
          detail::rtrim(variable, loc);
          detail::ltrim(value, loc);
          auto & sec = sections[section];
          if (sec.find(variable) == sec.end())
            sec.insert(std::make_pair(variable, value));
          else
            errors.push_back(line);
        }
        else {
          errors.push_back(line);
        }
      }
    }
  }

  void interpolate() {
    int global_iteration = 0;
    auto changed = false;
    // replace each "${variable}" by "${section:variable}"
    for (auto & sec : sections)
      replace_symbols(local_symbols(sec.first, sec.second), sec.second);
    // replace each "${section:variable}" by its value
    do {
      changed = false;
      const auto syms = global_symbols();
      for (auto & sec : sections)
        changed |= replace_symbols(syms, sec.second);
    } while (changed && (max_interpolation_depth > global_iteration++));
  }

  void default_section(const Section & sec) {
    for (auto & sec2 : sections)
      for (const auto & val : sec)
        sec2.second.insert(val);
  }

  void strip_trailing_comments() {
    const std::locale loc{ "C" };
    for (auto & sec : sections)
      for (auto & val : sec.second) {
        detail::rtrim2(val.second, [this](char ch) { return format->is_comment(ch); });
        detail::rtrim(val.second, loc);
      }
  }

  void clear() {
    sections.clear();
    errors.clear();
  }

private:
  using Symbols = std::list<std::pair<std::string, std::string>>;

  const Symbols local_symbols(const std::string & sec_name, const Section & sec) const {
    Symbols result;
    for (const auto & val : sec)
      result.push_back(std::make_pair(format->local_symbol(val.first), format->global_symbol(sec_name, val.first)));
    return result;
  }

  const Symbols global_symbols() const {
    Symbols result;
    for (const auto & sec : sections)
      for (const auto & val : sec.second)
        result.push_back(
          std::make_pair(format->global_symbol(sec.first, val.first), val.second));
    return result;
  }

  bool replace_symbols(const Symbols & syms, Section & sec) const {
    auto changed = false;
    for (auto & sym : syms)
      for (auto & val : sec)
        changed |= detail::replace(val.second, sym.first, sym.second);
    return changed;
  }
};

}; // namespace skywing::config

#endif
