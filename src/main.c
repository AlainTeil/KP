#define POSIX_C_SOURCE 200809L

#include "knapsack/knapsack.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t CAP_LINE_MAX = 256U;
static const size_t ITEM_LINE_MAX = 8192U;
static const size_t INITIAL_ITEM_CAP = 16U;
static const int DECIMAL_BASE = 10;
static const size_t USAGE_BUF_SIZE = 128U;

#if CAP_LINE_MAX > INT_MAX
#error CAP_LINE_MAX must fit in int
#endif
#if ITEM_LINE_MAX > INT_MAX
#error ITEM_LINE_MAX must fit in int
#endif

static bool has_only_trailing_space(const char *cursor) {
  while (*cursor != '\0') {
    if (*cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n') {
      return false;
    }
    ++cursor;
  }
  return true;
}

static int read_line_into(char *buffer, size_t buffer_size, FILE *file) {
  if (!buffer || buffer_size == 0U || !file) {
    return -1;
  }
  if (!fgets(buffer, (int)buffer_size, file)) {
    return -1;
  }
  if (strchr(buffer, '\n') == NULL && !feof(file)) {
    return -1; /* line too long */
  }
  return 0;
}

static int parse_capacity(FILE *file, int *capacity) {
  char line[CAP_LINE_MAX];
  if (read_line_into(line, sizeof line, file) != 0) {
    return -1;
  }

  char *endptr = NULL;
  errno = 0;
  long cap = strtol(line, &endptr, DECIMAL_BASE);
  if (errno != 0 || endptr == line || cap < 0 || cap > INT_MAX) {
    return -1;
  }
  if (!has_only_trailing_space(endptr)) {
    return -1;
  }
  *capacity = (int)cap;
  return 0;
}

static bool parse_item_token(char *token, int *weight_out, int *value_out) {
  if (!token || !weight_out || !value_out) {
    return false;
  }
  char *colon = strchr(token, ':');
  if (!colon || colon == token || colon[1] == '\0') {
    return false;
  }
  *colon = '\0';
  char *weight_str = token;
  char *value_str = colon + 1;

  errno = 0;
  char *endptr = NULL;
  long weight_val = strtol(weight_str, &endptr, DECIMAL_BASE);
  if (errno != 0 || endptr == weight_str || *endptr != '\0' || weight_val < 0 ||
      weight_val > INT_MAX) {
    return false;
  }

  errno = 0;
  endptr = NULL;
  long value_val = strtol(value_str, &endptr, DECIMAL_BASE);
  if (errno != 0 || endptr == value_str || *endptr != '\0' || value_val < INT_MIN ||
      value_val > INT_MAX) {
    return false;
  }

  *weight_out = (int)weight_val;
  *value_out = (int)value_val;
  return true;
}

static bool ensure_item_capacity(knapsack_item_t **items, size_t *capacity, size_t count) {
  if (!items || !capacity) {
    return false;
  }
  if (count < *capacity) {
    return true;
  }
  if (*capacity > SIZE_MAX / 2U) {
    return false;
  }
  size_t new_capacity = *capacity * 2U;
  if (new_capacity > SIZE_MAX / sizeof(knapsack_item_t)) {
    return false;
  }
  knapsack_item_t *tmp = (knapsack_item_t *)realloc(*items, new_capacity * sizeof(knapsack_item_t));
  if (!tmp) {
    return false;
  }
  *items = tmp;
  *capacity = new_capacity;
  return true;
}

static int parse_items(FILE *file, knapsack_item_t **items_out, size_t *count_out) {
  char line[ITEM_LINE_MAX];
  if (read_line_into(line, sizeof line, file) != 0) {
    return -1;
  }

  size_t capacity = INITIAL_ITEM_CAP;
  size_t count = 0U;
  knapsack_item_t *items = (knapsack_item_t *)malloc(capacity * sizeof(knapsack_item_t));
  if (!items) {
    return -1;
  }

  const char *delims = " ,\t\r\n";
  bool parse_ok = true;
  for (char *token = strtok(line, delims); token != NULL; token = strtok(NULL, delims)) {
    int weight = 0;
    int value = 0;
    if (!parse_item_token(token, &weight, &value)) {
      parse_ok = false;
      break;
    }
    if (!ensure_item_capacity(&items, &capacity, count)) {
      parse_ok = false;
      break;
    }
    items[count].weight = weight;
    items[count].value = value;
    ++count;
  }

  if (!parse_ok || count == 0U) {
    free(items);
    return -1;
  }

  *items_out = items;
  *count_out = count;
  return 0;
}

static void print_result(const knapsack_result_t *result) {
  printf("Optimal value: %d\n", result->optimal_value);
  printf("Selected indices (%zu):", result->selected_count);
  for (size_t i = 0; i < result->selected_count; ++i) {
    printf(" %zu", result->selected_indices[i]);
  }
  printf("\n");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fputs("Usage: ", stderr);
    fputs(argv[0], stderr);
    fputs(" <input_file>\n", stderr);
    return EXIT_FAILURE;
  }

  const char *path = argv[1];
  FILE *file = fopen(path, "r");
  if (!file) {
    perror("Failed to open input file");
    return EXIT_FAILURE;
  }

  int capacity = 0;
  if (parse_capacity(file, &capacity) != 0) {
    fputs("Failed to parse capacity\n", stderr);
    fclose(file);
    return EXIT_FAILURE;
  }

  knapsack_item_t *items = NULL;
  size_t count = 0;
  if (parse_items(file, &items, &count) != 0) {
    fputs("Failed to parse items\n", stderr);
    fclose(file);
    return EXIT_FAILURE;
  }
  fclose(file);

  knapsack_result_t result;
  if (!knapsack_solve(items, count, capacity, &result)) {
    fputs("Knapsack solve failed\n", stderr);
    free(items);
    return EXIT_FAILURE;
  }

  print_result(&result);

  knapsack_result_free(&result);
  free(items);
  return EXIT_SUCCESS;
}