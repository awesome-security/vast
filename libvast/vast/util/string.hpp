#ifndef VAST_UTIL_STRING_HPP
#define VAST_UTIL_STRING_HPP

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "vast/util/assert.hpp"
#include "vast/util/coding.hpp"

namespace vast {
namespace util {

/// Unscapes a string according to an escaper.
/// @param str The string to escape.
/// @param escaper The escaper to use.
/// @returns The escaped version of *str*.
template <class Escaper>
std::string escape(std::string const& str, Escaper escaper) {
  std::string result;
  result.reserve(str.size());
  auto f = str.begin();
  auto l = str.end();
  auto out = std::back_inserter(result);
  while (f != l)
    escaper(f, l, out);
  return result;
}

/// Unscapes a string according to an unescaper.
/// @param str The string to unescape.
/// @param unescaper The unescaper to use.
/// @returns The unescaped version of *str*.
template <class Unescaper>
std::string unescape(std::string const& str, Unescaper unescaper) {
  std::string result;
  result.reserve(str.size());
  auto f = str.begin();
  auto l = str.end();
  auto out = std::back_inserter(result);
  while (f != l)
    if (!unescaper(f, l, out))
      return {};
  return result;
}

auto hex_escaper = [](auto& f, auto, auto out) {
  auto hex = byte_to_hex(*f++);
  *out++ = '\\';
  *out++ = 'x';
  *out++ = hex.first;
  *out++ = hex.second;
};

auto hex_unescaper = [](auto& f, auto l, auto out) {
  auto hi = *f++;
  if (f == l)
    return false;
  auto lo = *f++;
  if (! std::isxdigit(hi) || ! std::isxdigit(lo))
    return false;
  *out++ = hex_to_byte(hi, lo);
  return true;
};

auto print_escaper = [](auto& f, auto l, auto out) {
  if (std::isprint(*f))
    *out++ = *f++;
  else
    hex_escaper(f, l, out);
};

auto byte_unescaper = [](auto& f, auto l, auto out) {
  if (*f != '\\') {
    *out++ = *f++;
    return true;
  }
  if (l - f < 4)
    return false; // Not enough input.
  if (*++f != 'x') {
    *out++ = *f++; // Remove escape backslashes that aren't \x.
    return true;
  }
  return hex_unescaper(++f, l, out);
};

// The JSON RFC (http://www.ietf.org/rfc/rfc4627.txt) specifies the escaping
// rules in section 2.5:
//
//    All Unicode characters may be placed within the quotation marks except
//    for the characters that must be escaped: quotation mark, reverse
//    solidus, and the control characters (U+0000 through U+001F).
//
// That is, '"', '\\', and control characters are the only mandatory escaped
// values. The rest is optional.
auto json_escaper = [](auto& f, auto l, auto out) {
  auto escape_char = [](char c, auto out) {
    *out++ = '\\';
    *out++ = c;
  };
  auto json_print_escaper = [](auto& f, auto, auto out) {
    if (std::isprint(*f)) {
      *out++ = *f++;
    } else {
      auto hex = byte_to_hex(*f++);
      *out++ = '\\';
      *out++ = 'u';
      *out++ = '0';
      *out++ = '0';
      *out++ = hex.first;
      *out++ = hex.second;
    }
  };
  switch (*f) {
    default:
      json_print_escaper(f, l, out);
      return;
    case '"':
    case '\\':
      escape_char(*f, out);
      break;
    case '\b':
      escape_char('b', out);
      break;
    case '\f':
      escape_char('f', out);
      break;
    case '\r':
      escape_char('r', out);
      break;
    case '\n':
      escape_char('n', out);
      break;
    case '\t':
      escape_char('t', out);
      break;
  }
  ++f;
};

auto json_unescaper = [](auto& f, auto l, auto out) {
  if (*f == '"') // Unescaped double-quotes not allowed.
    return false;
  if (*f != '\\') { // Skip every non-escape character.
    *out++ = *f++;
    return true;
  }
  if (l - f < 2)
    return false; // Need at least one char after \.
  switch (auto c = *++f) {
    default:
      return false;
    case '\\':
      *out++ = '\\';
      break;
    case '"':
      *out++ = '"';
      break;
    case '/':
      *out++ = '/';
      break;
    case 'b':
      *out++ = '\b';
      break;
    case 'f':
      *out++ = '\f';
      break;
    case 'r':
      *out++ = '\r';
      break;
    case 'n':
      *out++ = '\n';
      break;
    case 't':
      *out++ = '\t';
      break;
    case 'u': {
      // We currently only support single-byte escapings and any unicode escape
      // sequence other than \u00XX as is.
      if (l - f < 4)
        return false;
      std::array<char, 4> bytes;
      bytes[0] = *++f;
      bytes[1] = *++f;
      bytes[2] = *++f;
      bytes[3] = *++f;
      if (bytes[0] != '0' || bytes[1] != '0') {
        // Leave input as is, we don't know how to handle it (yet).
        *out++ = '\\';
        *out++ = 'u';
        std::copy(bytes.begin(), bytes.end(), out);
      } else {
        // Hex-unescape the XX portion of \u00XX.
        if (! std::isxdigit(bytes[2]) || ! std::isxdigit(bytes[3]))
          return false;
        *out++ = hex_to_byte(bytes[2], bytes[3]);
      }
      break;
    }
  }
  ++f;
  return true;
};

auto percent_escaper = [](auto& f, auto, auto out) {
  auto is_unreserved = [](char c) {
    return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
  };
  if (is_unreserved(*f)) {
    *out++ = *f++;
  } else {
    auto hex = byte_to_hex(*f++);
    *out++ = '%';
    *out++ = hex.first;
    *out++ = hex.second;
  }
};

auto percent_unescaper = [](auto& f, auto l, auto out) {
  if (*f != '%') {
    *out++ = *f++;
    return true;
  }
  if (l - f < 3) // Need %xx
    return false;
  return hex_unescaper(++f, l, out);
};

auto double_escaper = [](std::string const& esc) {
  return [&](auto& f, auto, auto out) {
    if (esc.find(*f) != std::string::npos)
      *out++ = *f;
    *out++ = *f++;
  };
};

auto double_unescaper = [](std::string const& esc) {
  return [&](auto& f, auto l, auto out) -> bool {
    auto x = *f++;
    if (f == l) {
      *out++ = x;
      return true;
    }
    *out++ = x;
    auto y = *f++;
    if (x == y && esc.find(x) == std::string::npos)
      *out++ = y;
    return true;
  };
};

/// Escapes all non-printable characters in a string with `\xAA` where `AA` is
/// the byte in hexadecimal representation.
/// @param str The string to escape.
/// @returns The escaped string of *str*.
/// @relates bytes_escape_all byte_unescape
std::string byte_escape(std::string const& str);

/// Escapes all non-printable characters in a string with `\xAA` where `AA` is
/// the byte in hexadecimal representation, plus a given list of extra
/// characters to escape.
/// @param str The string to escape.
/// @param extra The extra characters to escape.
/// @returns The escaped string of *str*.
/// @relates bytes_escape_all byte_unescape
std::string byte_escape(std::string const& str, std::string const& extra);

/// Escapes all characters in a string with `\xAA` where `AA` is
/// the byte in hexadecimal representation of the character.
/// @param str The string to escape.
/// @returns The escaped string of *str*.
/// @relates byte_unescape
std::string byte_escape_all(std::string const& str);

/// Unescapes a byte-escaped string, i.e., replaces all occurrences of `\xAA`
/// with the value of the byte `AA`.
/// @param str The string to unescape.
/// @returns The unescaped string of *str*.
/// @relates byte_escape bytes_escape_all
std::string byte_unescape(std::string const& str);

/// Escapes a string according to JSON escaping.
/// @param str The string to escape.
/// @returns The escaped string.
/// @relates json_unescape
std::string json_escape(std::string const& str);

/// Unescapes a string escaped with JSON escaping.
/// @param str The string to unescape.
/// @returns The unescaped string.
/// @relates json_escape
std::string json_unescape(std::string const& str);

/// Escapes a string according to percent-encoding.
/// @note This function escapes all non-*unreserved* characters as listed in
///       RFC3986. It does *not* correctly preserve HTTP URLs, but servers
///       merely as poor-man's substitute to prevent illegal characters from
///       slipping in.
/// @param str The string to escape.
/// @returns The escaped string.
/// @relates percent_unescape
std::string percent_escape(std::string const& str);

/// Unescapes a percent-encoded string.
/// @param str The string to unescape.
/// @returns The unescaped string.
/// @relates percent_escape
std::string percent_unescape(std::string const& str);

/// Escapes a string by repeating characters from a special set.
/// @param str The string to escape.
/// @param esc The set of characters to double-escape.
/// @returns The escaped string.
/// @relates double_unescape
std::string double_escape(std::string const& str, std::string const& esc);

/// Unescapes a string by removing consecutive character sequences.
/// @param str The string to unescape.
/// @param esc The set of repeated characters to unescape.
/// @returns The unescaped string.
/// @relates double_escape
std::string double_unescape(std::string const& str, std::string const& esc);

/// Replaces find and replace all occurences of a substring.
/// @param str The string in which to replace a substring.
/// @param search The string to search.
/// @param replace The replacement string.
/// @returns The string with replacements.
std::string replace_all(std::string str, const std::string& search,
                        const std::string& replace);

/// Splits a string into a vector of iterator pairs representing the
/// *[start, end)* range of each element.
/// @tparam Iterator A random-access iterator to a character sequence.
/// @param begin The beginning of the string to split.
/// @param end The end of the string to split.
/// @param sep The seperator where to split.
/// @param esc The escape string. If *esc* occurrs immediately in front of
///            *sep*, then *sep* will not count as a separator.
/// @param max_splits The maximum number of splits to perform.
/// @param include_sep If `true`, also include the separator after each
///                    match.
/// @pre `! sep.empty()`
/// @returns A vector of iterator pairs each of which delimit a single field
///          with a range *[start, end)*.
template <typename Iterator>
std::vector<std::pair<Iterator, Iterator>>
split(Iterator begin, Iterator end, std::string const& sep,
      std::string const& esc = "", size_t max_splits = -1,
      bool include_sep = false) {
  VAST_ASSERT(!sep.empty());
  std::vector<std::pair<Iterator, Iterator>> pos;
  size_t splits = 0;
  auto i = begin;
  auto prev = i;
  while (i != end) {
    // Find a separator that fits in the string.
    if (*i != sep[0] || i + sep.size() > end) {
      ++i;
      continue;
    }
    // Check remaining separator characters.
    size_t j = 1;
    auto s = i;
    while (j < sep.size())
      if (*++s != sep[j])
        break;
      else
        ++j;
    // No separator match.
    if (j != sep.size()) {
      ++i;
      continue;
    }
    // Make sure it's not an escaped match.
    if (!esc.empty() && esc.size() < static_cast<size_t>(i - begin)) {
      auto escaped = true;
      auto esc_start = i - esc.size();
      for (size_t j = 0; j < esc.size(); ++j)
        if (esc_start[j] != esc[j]) {
          escaped = false;
          break;
        }
      if (escaped) {
        ++i;
        continue;
      }
    }
    if (splits++ == max_splits)
      break;
    pos.emplace_back(prev, i);
    if (include_sep)
      pos.emplace_back(i, i + sep.size());
    i += sep.size();
    prev = i;
  }
  if (prev != end)
    pos.emplace_back(prev, end);
  return pos;
}

std::vector<std::pair<std::string::const_iterator, std::string::const_iterator>>
inline split(std::string const& str, std::string const& sep,
             std::string const& esc = "", size_t max_splits = -1,
             bool include_sep = false) {
  return split(str.begin(), str.end(), sep, esc, max_splits, include_sep);
}

/// Constructs a `std::vector<std::string>` from a ::split result.
/// @param v The vector of iterator pairs from ::split.
/// @returns a vector of strings with the split elements.
template <typename Iterator>
auto to_strings(std::vector<std::pair<Iterator, Iterator>> const& v) {
  std::vector<std::string> strs;
  strs.resize(v.size());
  for (size_t i = 0; i < v.size(); ++i)
    strs[i] = {v[i].first, v[i].second};
  return strs;
}

/// Combines ::split and ::to_strings.
template <typename Iterator>
auto split_to_str(Iterator begin, Iterator end, std::string const& sep,
                  std::string const& esc = "", size_t max_splits = -1,
                  bool include_sep = false) {
  return to_strings(split(begin, end, sep, esc, max_splits, include_sep));
}

inline auto split_to_str(std::string const& str, std::string const& sep,
                         std::string const& esc = "", size_t max_splits = -1,
                         bool include_sep = false) {
  return split_to_str(str.begin(), str.end(), sep, esc, max_splits,
                      include_sep);
}

/// Joins a sequence of strings according to a seperator.
/// @param begin The beginning of the sequence.
/// @param end The end of the sequence.
/// @param sep The string to insert between each element of the sequence.
/// @returns The joined string.
template <typename Iterator, typename Predicate>
std::string join(Iterator begin, Iterator end, std::string const& sep,
                 Predicate p) {
  std::string result;
  if (begin != end)
    result += p(*begin++);
  while (begin != end)
    result += sep + p(*begin++);
  return result;
}

template <typename Iterator>
std::string join(Iterator begin, Iterator end, std::string const& sep) {
  return join(begin, end, sep, [](auto&& x) -> decltype(x) { return x; });
}

template <typename T>
std::string join(std::vector<T> const& v, std::string const& sep) {
  return join(v.begin(), v.end(), sep);
}

/// Determines whether a string occurs at the beginning of another.
/// @param begin The beginning of the string.
/// @param end The end of the string.
/// @param str The substring to check at the start of *[begin, end)*.
/// @returns `true` iff *str* occurs at the beginning of *[begin, end)*.
template <typename Iterator>
bool starts_with(Iterator begin, Iterator end, std::string const& str) {
  using diff = typename std::iterator_traits<Iterator>::difference_type;
  if (static_cast<diff>(str.size()) > end - begin)
    return false;
  return std::equal(str.begin(), str.end(), begin);
}

inline bool starts_with(std::string const& str, std::string const& start) {
  return starts_with(str.begin(), str.end(), start);
}

/// Determines whether a string occurs at the end of another.
/// @param begin The beginning of the string.
/// @param end The end of the string.
/// @param str The substring to check at the end of *[begin, end)*.
/// @returns `true` iff *str* occurs at the end of *[begin, end)*.
template <typename Iterator>
bool ends_with(Iterator begin, Iterator end, std::string const& str) {
  using diff = typename std::iterator_traits<Iterator>::difference_type;
  return static_cast<diff>(str.size()) <= end - begin
         && std::equal(str.begin(), str.end(), end - str.size());
}

inline bool ends_with(std::string const& str, std::string const& end) {
  return ends_with(str.begin(), str.end(), end);
}

} // namespace util
} // namespace vast

#endif
