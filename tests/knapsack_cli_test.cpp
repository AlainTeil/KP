extern "C" {
#include "cli_internal.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

// RAII helper to wrap a string as a FILE* via fmemopen.
class StringFile {
public:
  explicit StringFile(const std::string &s) : data_(s) {
    file_ = fmemopen(const_cast<char *>(data_.data()), data_.size(), "r");
  }
  ~StringFile() {
    if (file_ != nullptr) {
      std::fclose(file_);
    }
  }
  StringFile(const StringFile &) = delete;
  StringFile &operator=(const StringFile &) = delete;
  FILE *get() { return file_; }

private:
  std::string data_;
  FILE *file_ = nullptr;
};

// RAII helper that captures writes to a tmpfile() stream.
class CapturedStream {
public:
  CapturedStream() : file_(std::tmpfile()) {}
  ~CapturedStream() {
    if (file_ != nullptr) {
      std::fclose(file_);
    }
  }
  CapturedStream(const CapturedStream &) = delete;
  CapturedStream &operator=(const CapturedStream &) = delete;
  FILE *get() { return file_; }
  std::string read_all() {
    std::fflush(file_);
    std::fseek(file_, 0, SEEK_SET);
    std::string out;
    char buf[256];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, file_)) > 0) {
      out.append(buf, n);
    }
    return out;
  }

private:
  FILE *file_ = nullptr;
};

} // namespace

TEST(CliParseCapacity, AcceptsValidNumber) {
  StringFile sf("42\n");
  int cap = -1;
  ASSERT_EQ(cli_parse_capacity(sf.get(), &cap), 0);
  EXPECT_EQ(cap, 42);
}

TEST(CliParseCapacity, RejectsNegative) {
  StringFile sf("-5\n");
  int cap = 0;
  EXPECT_EQ(cli_parse_capacity(sf.get(), &cap), -1);
}

TEST(CliParseCapacity, RejectsTrailingGarbage) {
  StringFile sf("12abc\n");
  int cap = 0;
  EXPECT_EQ(cli_parse_capacity(sf.get(), &cap), -1);
}

TEST(CliParseCapacity, RejectsEmptyLine) {
  StringFile sf("\n");
  int cap = 0;
  EXPECT_EQ(cli_parse_capacity(sf.get(), &cap), -1);
}

TEST(CliParseItems, AcceptsWhitespaceSeparated) {
  StringFile sf("1:2 3:4 5:6\n");
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  ASSERT_EQ(cli_parse_items(sf.get(), &items, &count), 0);
  ASSERT_EQ(count, 3U);
  EXPECT_EQ(items[0].weight, 1);
  EXPECT_EQ(items[0].value, 2);
  EXPECT_EQ(items[1].weight, 3);
  EXPECT_EQ(items[1].value, 4);
  EXPECT_EQ(items[2].weight, 5);
  EXPECT_EQ(items[2].value, 6);
  std::free(items);
}

TEST(CliParseItems, AcceptsCommaSeparated) {
  StringFile sf("1:2,3:4,5:6\n");
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  ASSERT_EQ(cli_parse_items(sf.get(), &items, &count), 0);
  EXPECT_EQ(count, 3U);
  std::free(items);
}

TEST(CliParseItems, RejectsMissingValue) {
  StringFile sf("1:\n");
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_items(sf.get(), &items, &count), -1);
  EXPECT_EQ(items, nullptr);
}

TEST(CliParseItems, RejectsZeroWeight) {
  StringFile sf("0:5\n");
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_items(sf.get(), &items, &count), -1);
}

TEST(CliParseItems, RejectsNegativeValue) {
  StringFile sf("1:-3\n");
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_items(sf.get(), &items, &count), -1);
}

TEST(CliParseItems, GrowsBeyondInitialCapacity) {
  std::string big;
  for (int i = 0; i < 32; ++i) {
    if (i > 0) {
      big.push_back(' ');
    }
    big += std::to_string(i + 1) + ":" + std::to_string(i);
  }
  big.push_back('\n');
  StringFile sf(big);
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  ASSERT_EQ(cli_parse_items(sf.get(), &items, &count), 0);
  EXPECT_EQ(count, 32U);
  std::free(items);
}

TEST(CliParseBuffer, AcceptsValidInput) {
  std::string input = "10\n1:2 3:4\n";
  int cap = 0;
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  ASSERT_EQ(cli_parse_buffer(input.data(), input.size(), &cap, &items, &count), 0);
  EXPECT_EQ(cap, 10);
  EXPECT_EQ(count, 2U);
  std::free(items);
}

TEST(CliParseBuffer, RejectsMissingNewline) {
  std::string input = "10";
  int cap = 0;
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_buffer(input.data(), input.size(), &cap, &items, &count), -1);
}

TEST(CliStatusToString, MapsAllCodes) {
  EXPECT_STREQ(cli_status_to_string(KNAPSACK_OK), "OK");
  EXPECT_STREQ(cli_status_to_string(KNAPSACK_ERR_NULL_RESULT), "NULL_RESULT");
  EXPECT_STREQ(cli_status_to_string(KNAPSACK_ERR_INVALID_ITEMS), "INVALID_ITEMS");
  EXPECT_STREQ(cli_status_to_string(KNAPSACK_ERR_TOO_MANY_ITEMS), "TOO_MANY_ITEMS");
  EXPECT_STREQ(cli_status_to_string(KNAPSACK_ERR_INVALID_CAPACITY), "INVALID_CAPACITY");
  EXPECT_STREQ(cli_status_to_string(KNAPSACK_ERR_DIMENSION_OVERFLOW), "DIMENSION_OVERFLOW");
  EXPECT_STREQ(cli_status_to_string(KNAPSACK_ERR_INT_OVERFLOW), "INT_OVERFLOW");
  EXPECT_STREQ(cli_status_to_string(KNAPSACK_ERR_ALLOC), "ALLOC");
}

TEST(CliJsonQuote, EscapesQuotesAndBackslashes) {
  CapturedStream cs;
  cli_json_quote(cs.get(), "he said \"hi\\bye\"");
  EXPECT_EQ(cs.read_all(), "he said \\\"hi\\\\bye\\\"");
}

TEST(CliJsonQuote, EscapesControlCharacters) {
  CapturedStream cs;
  const char input[] = {'a', '\n', 'b', '\t', 'c', '\b', 'd', '\f', 'e', '\r', 'f', '\0'};
  cli_json_quote(cs.get(), input);
  EXPECT_EQ(cs.read_all(), "a\\nb\\tc\\bd\\fe\\rf");
}

TEST(CliJsonQuote, EscapesUnicodeRange) {
  CapturedStream cs;
  const char input[] = {0x01, 0x1f, '\0'};
  cli_json_quote(cs.get(), input);
  EXPECT_EQ(cs.read_all(), "\\u0001\\u001f");
}

TEST(CliPrintErrorJson, ContainsCodeAndEscapedMessage) {
  CapturedStream cs;
  cli_print_error_json(cs.get(), "bad \"input\"", KNAPSACK_ERR_INVALID_CAPACITY);
  const std::string out = cs.read_all();
  EXPECT_NE(out.find("\"code\":\"INVALID_CAPACITY\""), std::string::npos);
  EXPECT_NE(out.find("\\\"input\\\""), std::string::npos);
}

TEST(CliStatusToString, UnknownCodeReturnsFallback) {
  // Cast a value outside the enum range to exercise the default branch.
  const int bogus = 9999;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto code = static_cast<knapsack_status_t>(bogus);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
  EXPECT_STREQ(cli_status_to_string(code), "UNKNOWN");
}

TEST(CliJsonQuote, NullTextIsNoOp) {
  CapturedStream cs;
  cli_json_quote(cs.get(), nullptr);
  EXPECT_EQ(cs.read_all(), "");
}

TEST(CliParseItems, RejectsNonNumericWeight) {
  StringFile sf("abc:5\n");
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_items(sf.get(), &items, &count), -1);
}

TEST(CliParseItems, RejectsNonNumericValue) {
  StringFile sf("5:xyz\n");
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_items(sf.get(), &items, &count), -1);
}

TEST(CliParseItems, RejectsTokenLongerThanInternalBuffer) {
  // parse_item_token uses a 64-byte local buffer; a token of 70+ chars must be rejected.
  std::string long_token(70, '1');
  long_token += ":2\n";
  StringFile sf(long_token);
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_items(sf.get(), &items, &count), -1);
}

TEST(CliParseBuffer, RejectsNullArguments) {
  int cap = 0;
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_buffer(nullptr, 0, &cap, &items, &count), -1);
  const char buf[] = "10\n1:2\n";
  EXPECT_EQ(cli_parse_buffer(buf, sizeof buf - 1, nullptr, &items, &count), -1);
  EXPECT_EQ(cli_parse_buffer(buf, sizeof buf - 1, &cap, nullptr, &count), -1);
  EXPECT_EQ(cli_parse_buffer(buf, sizeof buf - 1, &cap, &items, nullptr), -1);
}

TEST(CliParseBuffer, RejectsOversizedCapacityLine) {
  // KNAPSACK_CLI_CAP_LINE_MAX == 256; a 300-char capacity line must be rejected.
  std::string input(300, '1');
  input.push_back('\n');
  input += "1:2\n";
  int cap = 0;
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_buffer(input.data(), input.size(), &cap, &items, &count), -1);
}

TEST(CliParseBuffer, RejectsOversizedItemsLine) {
  // KNAPSACK_CLI_ITEM_LINE_MAX == 8192; build an items line larger than that.
  std::string input = "10\n";
  while (input.size() < 8200U) {
    input += "1:1 ";
  }
  int cap = 0;
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_buffer(input.data(), input.size(), &cap, &items, &count), -1);
}

TEST(CliParseBuffer, RejectsNegativeCapacity) {
  const char input[] = "-1\n1:2\n";
  int cap = 0;
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_buffer(input, sizeof input - 1, &cap, &items, &count), -1);
}

TEST(CliParseBuffer, RejectsTrailingGarbageInCapacity) {
  const char input[] = "10abc\n1:2\n";
  int cap = 0;
  knapsack_item_t *items = nullptr;
  size_t count = 0;
  EXPECT_EQ(cli_parse_buffer(input, sizeof input - 1, &cap, &items, &count), -1);
}
