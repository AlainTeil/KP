#include "cli_internal.h"

#include "knapsack/knapsack.h"

#include <stdio.h>

const char *cli_status_to_string(knapsack_status_t status) {
  switch (status) {
  case KNAPSACK_OK:
    return "OK";
  case KNAPSACK_ERR_NULL_RESULT:
    return "NULL_RESULT";
  case KNAPSACK_ERR_INVALID_ITEMS:
    return "INVALID_ITEMS";
  case KNAPSACK_ERR_TOO_MANY_ITEMS:
    return "TOO_MANY_ITEMS";
  case KNAPSACK_ERR_INVALID_CAPACITY:
    return "INVALID_CAPACITY";
  case KNAPSACK_ERR_DIMENSION_OVERFLOW:
    return "DIMENSION_OVERFLOW";
  case KNAPSACK_ERR_INT_OVERFLOW:
    return "INT_OVERFLOW";
  case KNAPSACK_ERR_ALLOC:
    return "ALLOC";
  }
  return "UNKNOWN";
}

void cli_json_quote(FILE *stream, const char *text) {
  if (!text) {
    return;
  }
  for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
    const unsigned char c = *p;
    switch (c) {
    case '"':
      fputs("\\\"", stream);
      break;
    case '\\':
      fputs("\\\\", stream);
      break;
    case '\b':
      fputs("\\b", stream);
      break;
    case '\f':
      fputs("\\f", stream);
      break;
    case '\n':
      fputs("\\n", stream);
      break;
    case '\r':
      fputs("\\r", stream);
      break;
    case '\t':
      fputs("\\t", stream);
      break;
    default:
      if (c < 0x20U) {
        fprintf(stream, "\\u%04x", c);
      } else {
        fputc((int)c, stream);
      }
      break;
    }
  }
}

void cli_print_result_text(const knapsack_result_t *result) {
  printf("Optimal value: %d\n", result->optimal_value);
  printf("Selected indices (%zu):", result->selected_count);
  for (size_t i = 0; i < result->selected_count; ++i) {
    printf(" %zu", result->selected_indices[i]);
  }
  printf("\n");
}

void cli_print_result_json(const knapsack_result_t *result) {
  printf("{\"status\":\"ok\",\"optimal_value\":%d,\"selected_indices\":[", result->optimal_value);
  for (size_t i = 0; i < result->selected_count; ++i) {
    if (i > 0U) {
      fputc(',', stdout);
    }
    printf("%zu", result->selected_indices[i]);
  }
  fputs("]}\n", stdout);
}

void cli_print_error_text(FILE *stream, const char *message) {
  if (message) {
    fputs(message, stream);
  }
  fputc('\n', stream);
}

void cli_print_error_json(FILE *stream, const char *message, knapsack_status_t status) {
  fputs("{\"status\":\"error\",\"code\":\"", stream);
  fputs(cli_status_to_string(status), stream);
  fputs("\",\"message\":\"", stream);
  cli_json_quote(stream, message ? message : "");
  fputs("\"}\n", stream);
}
