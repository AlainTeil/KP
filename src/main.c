#define _POSIX_C_SOURCE 200809L

#include "cli/cli_internal.h"
#include "knapsack/knapsack.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(FILE *stream, const char *prog) {
  fprintf(stream,
          "Usage: %s [--json] <input_file>\n"
          "       %s --help | -h\n"
          "       %s --version\n"
          "\n"
          "Options:\n"
          "  --json       Emit machine-readable JSON output.\n"
          "  -h, --help   Show this help message and exit.\n"
          "  --version    Print version and exit.\n"
          "\n"
          "Input file format:\n"
          "  line 1: capacity (integer >= 0)\n"
          "  line 2: whitespace/comma-separated weight:value pairs\n",
          prog, prog, prog);
}

static void print_version(void) {
  printf("knapsack_demo %d.%d.%d\n", KNAPSACK_VERSION_MAJOR, KNAPSACK_VERSION_MINOR,
         KNAPSACK_VERSION_PATCH);
}

static void emit_error(bool json_mode, const char *message, knapsack_status_t status) {
  if (json_mode) {
    cli_print_error_json(stdout, message, status);
  } else {
    cli_print_error_text(stderr, message);
  }
}

int main(int argc, char **argv) {
  bool json_mode = false;
  const char *path = NULL;
  const char *prog = (argc > 0 && argv[0]) ? argv[0] : "knapsack_demo";

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      print_usage(stdout, prog);
      return EXIT_SUCCESS;
    }
    if (strcmp(arg, "--version") == 0) {
      print_version();
      return EXIT_SUCCESS;
    }
    if (strcmp(arg, "--json") == 0) {
      json_mode = true;
      continue;
    }
    if (arg[0] == '-' && arg[1] != '\0') {
      fprintf(stderr, "Unknown option: %s\n", arg);
      print_usage(stderr, prog);
      return EXIT_FAILURE;
    }
    if (path != NULL) {
      fprintf(stderr, "Unexpected extra argument: %s\n", arg);
      print_usage(stderr, prog);
      return EXIT_FAILURE;
    }
    path = arg;
  }

  if (!path) {
    print_usage(stderr, prog);
    return EXIT_FAILURE;
  }

  FILE *file = fopen(path, "r");
  if (!file) {
    if (json_mode) {
      cli_print_error_json(stdout, "Failed to open input file", KNAPSACK_ERR_INVALID_ITEMS);
    } else {
      perror("Failed to open input file");
    }
    return EXIT_FAILURE;
  }

  int capacity = 0;
  if (cli_parse_capacity(file, &capacity) != 0) {
    emit_error(json_mode, "Failed to parse capacity", KNAPSACK_ERR_INVALID_CAPACITY);
    fclose(file);
    return EXIT_FAILURE;
  }

  knapsack_item_t *items = NULL;
  size_t count = 0;
  if (cli_parse_items(file, &items, &count) != 0) {
    emit_error(json_mode, "Failed to parse items", KNAPSACK_ERR_INVALID_ITEMS);
    fclose(file);
    return EXIT_FAILURE;
  }
  fclose(file);

  knapsack_result_t result;
  const knapsack_status_t status = knapsack_solve_status(items, count, capacity, &result);
  if (status != KNAPSACK_OK) {
    emit_error(json_mode, "Knapsack solve failed", status);
    free(items);
    return EXIT_FAILURE;
  }

  if (json_mode) {
    cli_print_result_json(&result);
  } else {
    cli_print_result_text(&result);
  }

  knapsack_result_free(&result);
  free(items);
  return EXIT_SUCCESS;
}
