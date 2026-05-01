#include <gtest/gtest.h>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef KNAPSACK_DEMO_PATH
#error "KNAPSACK_DEMO_PATH must be defined"
#endif

namespace {

// Minimal JSON value & parser used to validate the CLI's --json output.
// Supports the limited grammar the CLI emits: object root, with values
// that are strings, signed integers, or arrays of integers.
// NOLINTBEGIN(readability-braces-around-statements,readability-implicit-bool-conversion,bugprone-narrowing-conversions,readability-isolate-declaration)
struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;
struct JsonValue {
  std::variant<std::monostate, long long, std::string, JsonArray, JsonObject> v;
  bool is_int() const { return std::holds_alternative<long long>(v); }
  bool is_str() const { return std::holds_alternative<std::string>(v); }
  bool is_arr() const { return std::holds_alternative<JsonArray>(v); }
  bool is_obj() const { return std::holds_alternative<JsonObject>(v); }
  long long as_int() const { return std::get<long long>(v); }
  const std::string &as_str() const { return std::get<std::string>(v); }
  const JsonArray &as_arr() const { return std::get<JsonArray>(v); }
  const JsonObject &as_obj() const { return std::get<JsonObject>(v); }
};

class JsonParser {
public:
  explicit JsonParser(const std::string &s) : s_(s) {}
  JsonValue parse() {
    skip_ws();
    JsonValue v = parse_value();
    skip_ws();
    if (pos_ != s_.size())
      throw std::runtime_error("trailing garbage in JSON");
    return v;
  }

private:
  void skip_ws() {
    while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_])))
      ++pos_;
  }
  char peek() const {
    if (pos_ >= s_.size())
      throw std::runtime_error("unexpected end of JSON");
    return s_[pos_];
  }
  char consume() {
    if (pos_ >= s_.size())
      throw std::runtime_error("unexpected end of JSON");
    return s_[pos_++];
  }
  void expect(char c) {
    if (consume() != c)
      throw std::runtime_error(std::string("expected '") + c + "'");
  }
  JsonValue parse_value() {
    skip_ws();
    char c = peek();
    if (c == '{')
      return parse_object();
    if (c == '[')
      return parse_array();
    if (c == '"')
      return JsonValue{parse_string()};
    if (c == '-' || (c >= '0' && c <= '9'))
      return parse_number();
    throw std::runtime_error("unexpected JSON token");
  }
  JsonValue parse_object() {
    expect('{');
    JsonObject obj;
    skip_ws();
    if (peek() == '}') {
      ++pos_;
      return JsonValue{std::move(obj)};
    }
    for (;;) {
      skip_ws();
      std::string k = parse_string();
      skip_ws();
      expect(':');
      JsonValue v = parse_value();
      obj.emplace(std::move(k), std::move(v));
      skip_ws();
      char c = consume();
      if (c == ',')
        continue;
      if (c == '}')
        break;
      throw std::runtime_error("expected ',' or '}'");
    }
    return JsonValue{std::move(obj)};
  }
  JsonValue parse_array() {
    expect('[');
    JsonArray arr;
    skip_ws();
    if (peek() == ']') {
      ++pos_;
      return JsonValue{std::move(arr)};
    }
    for (;;) {
      arr.push_back(parse_value());
      skip_ws();
      char c = consume();
      if (c == ',')
        continue;
      if (c == ']')
        break;
      throw std::runtime_error("expected ',' or ']'");
    }
    return JsonValue{std::move(arr)};
  }
  std::string parse_string() {
    expect('"');
    std::string out;
    while (pos_ < s_.size()) {
      char c = s_[pos_++];
      if (c == '"')
        return out;
      if (c == '\\') {
        if (pos_ >= s_.size())
          throw std::runtime_error("bad escape");
        char e = s_[pos_++];
        switch (e) {
        case '"':
          out.push_back('"');
          break;
        case '\\':
          out.push_back('\\');
          break;
        case '/':
          out.push_back('/');
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u':
          if (pos_ + 4 > s_.size())
            throw std::runtime_error("bad \\u escape");
          // Tests only need to round-trip ASCII; just keep the literal escape.
          out.push_back('\\');
          out.push_back('u');
          for (int i = 0; i < 4; ++i)
            out.push_back(s_[pos_++]);
          break;
        default:
          throw std::runtime_error("bad escape");
        }
      } else {
        out.push_back(c);
      }
    }
    throw std::runtime_error("unterminated string");
  }
  JsonValue parse_number() {
    size_t start = pos_;
    if (s_[pos_] == '-')
      ++pos_;
    while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9')
      ++pos_;
    if (pos_ < s_.size() && (s_[pos_] == '.' || s_[pos_] == 'e' || s_[pos_] == 'E')) {
      throw std::runtime_error("CLI never emits non-integer JSON numbers");
    }
    return JsonValue{std::stoll(s_.substr(start, pos_ - start))};
  }

  const std::string &s_;
  size_t pos_ = 0;
};

JsonValue parse_json(const std::string &s) { return JsonParser(s).parse(); }
// NOLINTEND(readability-braces-around-statements,readability-implicit-bool-conversion,bugprone-narrowing-conversions,readability-isolate-declaration)

// RAII temp-file: writes content on construction, removes on destruction.
class TempFile {
public:
  explicit TempFile(const std::string &content) {
    char tmpl[] = "/tmp/knapsack_demo_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd == -1) {
      throw std::system_error(errno, std::generic_category(), "mkstemp failed");
    }
    path_ = tmpl;
    const ssize_t written = write(fd, content.data(), content.size());
    close(fd);
    if (written < 0 || static_cast<size_t>(written) != content.size()) {
      std::filesystem::remove(path_);
      throw std::system_error(errno, std::generic_category(), "write failed");
    }
  }
  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }
  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
  const std::string &path() const { return path_; }

private:
  std::string path_;
};

struct CommandResult {
  int exit_code;
  std::string stdout_text;
  std::string stderr_text;
  std::string combined() const { return stdout_text + stderr_text; }
};

std::string drain_fd(int fd) {
  std::string out;
  char buf[256];
  for (;;) {
    ssize_t n = read(fd, buf, sizeof buf);
    if (n > 0) {
      out.append(buf, static_cast<size_t>(n));
    } else if (n == 0) {
      break;
    } else if (errno == EINTR) {
      continue;
    } else {
      throw std::system_error(errno, std::generic_category(), "read failed");
    }
  }
  return out;
}

CommandResult run_demo(const std::vector<std::string> &args) {
  int out_pipe[2];
  int err_pipe[2];
  if (pipe(out_pipe) == -1) {
    throw std::system_error(errno, std::generic_category(), "pipe");
  }
  if (pipe(err_pipe) == -1) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    throw std::system_error(errno, std::generic_category(), "pipe");
  }

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addclose(&actions, out_pipe[0]);
  posix_spawn_file_actions_addclose(&actions, err_pipe[0]);
  posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, err_pipe[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions, out_pipe[1]);
  posix_spawn_file_actions_addclose(&actions, err_pipe[1]);

  std::vector<char *> argv;
  std::string prog = KNAPSACK_DEMO_PATH;
  argv.push_back(prog.data());
  std::vector<std::string> storage = args; // own the strings
  for (auto &a : storage) {
    argv.push_back(a.data());
  }
  argv.push_back(nullptr);

  pid_t pid = 0;
  int rc = posix_spawn(&pid, KNAPSACK_DEMO_PATH, &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(out_pipe[1]);
  close(err_pipe[1]);

  if (rc != 0) {
    close(out_pipe[0]);
    close(err_pipe[0]);
    throw std::system_error(rc, std::generic_category(), "posix_spawn");
  }

  CommandResult result{-1, {}, {}};
  result.stdout_text = drain_fd(out_pipe[0]);
  result.stderr_text = drain_fd(err_pipe[0]);
  close(out_pipe[0]);
  close(err_pipe[0]);

  int status = 0;
  while (waitpid(pid, &status, 0) == -1) {
    if (errno != EINTR) {
      throw std::system_error(errno, std::generic_category(), "waitpid");
    }
  }
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return result;
}

} // namespace

TEST(KnapsackIntegrationTest, RunsDemoAndPrintsSelection) {
  TempFile input("10\n2:3 3:4 4:5 5:6\n");
  CommandResult r = run_demo({input.path()});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_NE(r.stdout_text.find("Optimal value: 13"), std::string::npos);
  EXPECT_NE(r.stdout_text.find("Selected indices (3): 0 1 3"), std::string::npos);
}

TEST(KnapsackIntegrationTest, JsonModePrintsMachineReadableOutput) {
  TempFile input("10\n2:3 3:4 4:5 5:6\n");
  CommandResult r = run_demo({"--json", input.path()});
  ASSERT_EQ(r.exit_code, 0) << r.stderr_text;
  JsonValue v = parse_json(r.stdout_text);
  ASSERT_TRUE(v.is_obj());
  const auto &o = v.as_obj();
  ASSERT_EQ(o.at("status").as_str(), "ok");
  ASSERT_EQ(o.at("optimal_value").as_int(), 13);
  const auto &idx = o.at("selected_indices").as_arr();
  ASSERT_EQ(idx.size(), 3U);
  EXPECT_EQ(idx[0].as_int(), 0);
  EXPECT_EQ(idx[1].as_int(), 1);
  EXPECT_EQ(idx[2].as_int(), 3);
  EXPECT_EQ(o.count("code"), 0U);
  EXPECT_EQ(o.count("message"), 0U);
}

TEST(KnapsackIntegrationTest, JsonFlagAcceptedAfterPath) {
  TempFile input("10\n2:3 3:4\n");
  CommandResult r = run_demo({input.path(), "--json"});
  ASSERT_EQ(r.exit_code, 0) << r.stderr_text;
  JsonValue v = parse_json(r.stdout_text);
  ASSERT_TRUE(v.is_obj());
  EXPECT_EQ(v.as_obj().at("status").as_str(), "ok");
}

TEST(KnapsackIntegrationTest, FailsGracefullyOnBadCapacity) {
  TempFile input("abc\n1:2\n");
  CommandResult r = run_demo({input.path()});
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.stderr_text.find("Failed to parse capacity"), std::string::npos);
}

TEST(KnapsackIntegrationTest, JsonModeBadCapacityEmitsErrorJson) {
  TempFile input("abc\n1:2\n");
  CommandResult r = run_demo({"--json", input.path()});
  EXPECT_NE(r.exit_code, 0);
  JsonValue v = parse_json(r.stdout_text);
  ASSERT_TRUE(v.is_obj());
  const auto &o = v.as_obj();
  EXPECT_EQ(o.at("status").as_str(), "error");
  EXPECT_EQ(o.at("code").as_str(), "INVALID_CAPACITY");
  EXPECT_TRUE(o.at("message").is_str());
  EXPECT_FALSE(o.at("message").as_str().empty());
  EXPECT_EQ(o.count("optimal_value"), 0U);
  EXPECT_EQ(o.count("selected_indices"), 0U);
}

TEST(KnapsackIntegrationTest, FailsOnTrailingJunkInCapacityLine) {
  TempFile input("10 extra\n1:2\n");
  CommandResult r = run_demo({input.path()});
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.stderr_text.find("Failed to parse capacity"), std::string::npos);
}

TEST(KnapsackIntegrationTest, FailsOnMalformedItem) {
  TempFile input("10\n2:3 3 4:5\n");
  CommandResult r = run_demo({input.path()});
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.stderr_text.find("Failed to parse items"), std::string::npos);
}

TEST(KnapsackIntegrationTest, FailsOnOverflowingNumber) {
  TempFile input("10\n999999999999:1\n");
  CommandResult r = run_demo({input.path()});
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.stderr_text.find("Failed to parse items"), std::string::npos);
}

TEST(KnapsackIntegrationTest, FailsOnMissingFile) {
  CommandResult r = run_demo({"/nonexistent/path/should/not/exist.txt"});
  EXPECT_NE(r.exit_code, 0);
}

TEST(KnapsackIntegrationTest, JsonMissingFileEmitsErrorJson) {
  CommandResult r = run_demo({"--json", "/nonexistent/path.txt"});
  EXPECT_NE(r.exit_code, 0);
  JsonValue v = parse_json(r.stdout_text);
  ASSERT_TRUE(v.is_obj());
  EXPECT_EQ(v.as_obj().at("status").as_str(), "error");
  EXPECT_TRUE(v.as_obj().at("message").is_str());
}

TEST(KnapsackIntegrationTest, HelpFlagSucceeds) {
  CommandResult r = run_demo({"--help"});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_NE(r.stdout_text.find("Usage:"), std::string::npos);
}

TEST(KnapsackIntegrationTest, ShortHelpFlagSucceeds) {
  CommandResult r = run_demo({"-h"});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_NE(r.stdout_text.find("Usage:"), std::string::npos);
}

TEST(KnapsackIntegrationTest, VersionFlagSucceeds) {
  CommandResult r = run_demo({"--version"});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_NE(r.stdout_text.find("knapsack_demo"), std::string::npos);
}

TEST(KnapsackIntegrationTest, UnknownFlagFails) {
  CommandResult r = run_demo({"--definitely-not-a-flag"});
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.stderr_text.find("Unknown option"), std::string::npos);
}

TEST(KnapsackIntegrationTest, NoArgumentsFails) {
  CommandResult r = run_demo({});
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.stderr_text.find("Usage:"), std::string::npos);
}

TEST(KnapsackIntegrationTest, ExtraPositionalArgumentFails) {
  TempFile input("10\n1:2\n");
  CommandResult r = run_demo({input.path(), "second-path"});
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.stderr_text.find("Unexpected extra argument"), std::string::npos);
}

TEST(KnapsackIntegrationTest, SolveErrorPropagatesExitCode) {
  // Capacity above KNAPSACK_MAX_CAPACITY (=100000) triggers an INVALID_CAPACITY
  // status from the solver, exercising the "Knapsack solve failed" branch in main.c.
  TempFile input("100001\n1:2\n");
  CommandResult r = run_demo({"--json", input.path()});
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.stdout_text.find("\"status\":\"error\""), std::string::npos);
  EXPECT_NE(r.stdout_text.find("INVALID_CAPACITY"), std::string::npos);
}

TEST(JsonParserSelfTest, ParsesBasicShapes) {
  JsonValue v = parse_json(R"({"status":"ok","optimal_value":13,"selected_indices":[0,1,3]})");
  ASSERT_TRUE(v.is_obj());
  EXPECT_EQ(v.as_obj().at("status").as_str(), "ok");
  EXPECT_EQ(v.as_obj().at("optimal_value").as_int(), 13);
  EXPECT_EQ(v.as_obj().at("selected_indices").as_arr().size(), 3U);
}

TEST(JsonParserSelfTest, RejectsTrailingGarbage) {
  EXPECT_THROW(parse_json("{}garbage"), std::runtime_error);
  EXPECT_THROW(parse_json("[1,2,]"), std::runtime_error);
  EXPECT_THROW(parse_json(""), std::runtime_error);
}

TEST(JsonParserSelfTest, HandlesEscapesInStrings) {
  JsonValue v = parse_json(R"({"message":"a\"b\\c\n"})");
  EXPECT_EQ(v.as_obj().at("message").as_str(), "a\"b\\c\n");
}
