#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef KNAPSACK_DEMO_PATH
#error "KNAPSACK_DEMO_PATH must be defined"
#endif

namespace {

std::string write_temp_file(const std::string &content) {
  char tmpl[] = "/tmp/knapsack_demo_XXXXXX";
  int file_descriptor = mkstemp(tmpl);
  if (file_descriptor == -1) {
    throw std::system_error(errno, std::generic_category(), "mkstemp failed");
  }
  const ssize_t written = write(file_descriptor, content.c_str(), content.size());
  close(file_descriptor);
  if (written < 0 || static_cast<size_t>(written) != content.size()) {
    throw std::system_error(errno, std::generic_category(), "write failed");
  }
  return std::string(tmpl);
}

struct CommandResult {
  int exit_code;
  std::string output;
};

constexpr size_t READ_BUFFER_SIZE = 256U;

CommandResult run_command(const std::string &cmd) {
  std::string output;
  std::string full_cmd = cmd + " 2>&1";
  FILE *pipe = popen(full_cmd.c_str(), "r");
  if (pipe == nullptr) {
    throw std::system_error(errno, std::generic_category(), "popen failed");
  }
  char buffer[READ_BUFFER_SIZE];
  while (fgets(buffer, sizeof buffer, pipe) != nullptr) {
    output.append(buffer);
  }
  int status = pclose(pipe);
  int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return {exit_code, output};
}

TEST(KnapsackIntegrationTest, RunsDemoAndPrintsSelection) {
  const std::string input = "10\n2:3 3:4 4:5 5:6\n";
  const std::string path = write_temp_file(input);

  const std::string command = std::string(KNAPSACK_DEMO_PATH) + " " + path;
  CommandResult result = run_command(command);

  std::filesystem::remove(path);

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_NE(result.output.find("Optimal value: 13"), std::string::npos);
  EXPECT_NE(result.output.find("Selected indices (3): 0 1 3"), std::string::npos);
}

TEST(KnapsackIntegrationTest, JsonModePrintsMachineReadableOutput) {
  const std::string input = "10\n2:3 3:4 4:5 5:6\n";
  const std::string path = write_temp_file(input);

  const std::string command = std::string(KNAPSACK_DEMO_PATH) + " --json " + path;
  CommandResult result = run_command(command);

  std::filesystem::remove(path);

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_NE(result.output.find("\"status\":\"ok\""), std::string::npos);
  EXPECT_NE(result.output.find("\"optimal_value\":13"), std::string::npos);
  EXPECT_NE(result.output.find("\"selected_indices\":[0,1,3]"), std::string::npos);
}

TEST(KnapsackIntegrationTest, FailsGracefullyOnBadInput) {
  const std::string input = "abc\n1:2\n"; // bad capacity
  const std::string path = write_temp_file(input);

  const std::string command = std::string(KNAPSACK_DEMO_PATH) + " " + path;
  CommandResult result = run_command(command);

  std::filesystem::remove(path);

  EXPECT_NE(result.exit_code, 0);
  EXPECT_NE(result.output.find("Failed to parse capacity"), std::string::npos);
}

TEST(KnapsackIntegrationTest, FailsOnTrailingJunkInCapacityLine) {
  const std::string input = "10 extra\n1:2\n"; // trailing token on capacity line
  const std::string path = write_temp_file(input);

  const std::string command = std::string(KNAPSACK_DEMO_PATH) + " " + path;
  CommandResult result = run_command(command);

  std::filesystem::remove(path);

  EXPECT_NE(result.exit_code, 0);
  EXPECT_NE(result.output.find("Failed to parse capacity"), std::string::npos);
}

TEST(KnapsackIntegrationTest, FailsOnMalformedItem) {
  const std::string input = "10\n2:3 3 4:5\n"; // missing colon on middle token
  const std::string path = write_temp_file(input);

  const std::string command = std::string(KNAPSACK_DEMO_PATH) + " " + path;
  CommandResult result = run_command(command);

  std::filesystem::remove(path);

  EXPECT_NE(result.exit_code, 0);
  EXPECT_NE(result.output.find("Failed to parse items"), std::string::npos);
}

TEST(KnapsackIntegrationTest, FailsOnOverflowingNumber) {
  const std::string input = "10\n999999999999:1\n"; // weight overflow
  const std::string path = write_temp_file(input);

  const std::string command = std::string(KNAPSACK_DEMO_PATH) + " " + path;
  CommandResult result = run_command(command);

  std::filesystem::remove(path);

  EXPECT_NE(result.exit_code, 0);
  EXPECT_NE(result.output.find("Failed to parse items"), std::string::npos);
}

} // namespace