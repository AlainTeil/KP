#define _POSIX_C_SOURCE 200809L

#include "cli_internal.h"

#include "knapsack/knapsack.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile-time guard: the line buffers must fit in fgets's int size argument.
 * These are now enum constants, so _Static_assert works as intended.
 */
_Static_assert(KNAPSACK_CLI_CAP_LINE_MAX > 0 && KNAPSACK_CLI_CAP_LINE_MAX <= INT_MAX,
               "CAP_LINE_MAX must fit in int");
_Static_assert(KNAPSACK_CLI_ITEM_LINE_MAX > 0 && KNAPSACK_CLI_ITEM_LINE_MAX <= INT_MAX,
               "ITEM_LINE_MAX must fit in int");

static const int DECIMAL_BASE = 10;

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

int cli_parse_capacity(FILE *file, int *capacity) {
  char line[KNAPSACK_CLI_CAP_LINE_MAX];
  if (read_line_into(line, sizeof line, file) != 0) {
    return -1;
  }
  errno = 0;
  char *endptr = NULL;
  const long parsed = strtol(line, &endptr, DECIMAL_BASE);
  if (errno != 0 || endptr == line || !has_only_trailing_space(endptr)) {
    return -1;
  }
  if (parsed < 0 || parsed > INT_MAX) {
    return -1;
  }
  *capacity = (int)parsed;
  return 0;
}

static bool parse_item_token(const char *token, int *weight_out, int *value_out) {
  const char *colon = strchr(token, ':');
  if (!colon || colon == token || colon[1] == '\0') {
    return false;
  }
  /* Make a local copy so we can NUL-terminate the weight side. */
  char buffer[64];
  const size_t token_len = strlen(token);
  if (token_len >= sizeof buffer) {
    return false;
  }
  memcpy(buffer, token, token_len + 1U);
  const size_t colon_offset = (size_t)(colon - token);
  buffer[colon_offset] = '\0';
  const char *const weight_str = buffer;
  const char *const value_str = buffer + colon_offset + 1U;

  errno = 0;
  char *endptr = NULL;
  const long weight_val = strtol(weight_str, &endptr, DECIMAL_BASE);
  if (errno != 0 || endptr == weight_str || *endptr != '\0') {
    return false;
  }
  if (weight_val <= 0 || weight_val > INT_MAX) {
    return false;
  }

  errno = 0;
  endptr = NULL;
  const long value_val = strtol(value_str, &endptr, DECIMAL_BASE);
  if (errno != 0 || endptr == value_str || *endptr != '\0') {
    return false;
  }
  if (value_val < 0 || value_val > INT_MAX) {
    return false;
  }

  *weight_out = (int)weight_val;
  *value_out = (int)value_val;
  return true;
}

static bool ensure_item_capacity(knapsack_item_t **items, size_t *capacity, size_t count) {
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
  knapsack_item_t *tmp = realloc(*items, new_capacity * sizeof(knapsack_item_t));
  if (!tmp) {
    return false;
  }
  *items = tmp;
  *capacity = new_capacity;
  return true;
}

/* Parse a NUL-terminated, mutable line into items. Returns 0 on success. */
static int parse_items_line(char *line, knapsack_item_t **items_out, size_t *count_out) {
  size_t capacity = (size_t)KNAPSACK_CLI_INITIAL_ITEM_CAP;
  size_t count = 0U;
  knapsack_item_t *items = malloc(capacity * sizeof(knapsack_item_t));
  if (!items) {
    return -1;
  }

  const char *delims = " ,\t\r\n";
  bool ok = true;
  char *saveptr = NULL;
  for (const char *token = strtok_r(line, delims, &saveptr); token != NULL;
       token = strtok_r(NULL, delims, &saveptr)) {
    int weight = 0;
    int value = 0;
    if (!parse_item_token(token, &weight, &value)) {
      ok = false;
      break;
    }
    if (!ensure_item_capacity(&items, &capacity, count)) {
      ok = false;
      break;
    }
    items[count].weight = weight;
    items[count].value = value;
    ++count;
  }

  if (!ok || count == 0U) {
    free(items);
    return -1;
  }
  *items_out = items;
  *count_out = count;
  return 0;
}

int cli_parse_items(FILE *file, knapsack_item_t **items_out, size_t *count_out) {
  char line[KNAPSACK_CLI_ITEM_LINE_MAX];
  if (read_line_into(line, sizeof line, file) != 0) {
    return -1;
  }
  return parse_items_line(line, items_out, count_out);
}

int cli_parse_buffer(const char *buffer, size_t length, int *capacity, knapsack_item_t **items,
                     size_t *count) {
  if (!buffer || !capacity || !items || !count) {
    return -1;
  }
  *items = NULL;
  *count = 0;

  /* Split into the first newline (capacity line) and the rest (items line). */
  const char *newline = memchr(buffer, '\n', length);
  if (!newline) {
    return -1;
  }
  const size_t cap_len = (size_t)(newline - buffer);
  if (cap_len + 1U >= (size_t)KNAPSACK_CLI_CAP_LINE_MAX) {
    return -1;
  }
  const size_t items_offset = cap_len + 1U;
  const size_t items_len = length - items_offset;
  if (items_len + 1U >= (size_t)KNAPSACK_CLI_ITEM_LINE_MAX) {
    return -1;
  }

  char cap_line[KNAPSACK_CLI_CAP_LINE_MAX];
  memcpy(cap_line, buffer, cap_len);
  cap_line[cap_len] = '\0';

  errno = 0;
  char *endptr = NULL;
  const long parsed = strtol(cap_line, &endptr, DECIMAL_BASE);
  if (errno != 0 || endptr == cap_line || !has_only_trailing_space(endptr)) {
    return -1;
  }
  if (parsed < 0 || parsed > INT_MAX) {
    return -1;
  }
  *capacity = (int)parsed;

  char items_line[KNAPSACK_CLI_ITEM_LINE_MAX];
  memcpy(items_line, buffer + items_offset, items_len);
  items_line[items_len] = '\0';
  return parse_items_line(items_line, items, count);
}
