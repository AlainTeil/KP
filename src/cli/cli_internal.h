/* Internal interface shared between src/cli/parse.c, src/cli/format.c and
 * src/main.c. NOT part of the public knapsack library API.
 */
#ifndef KNAPSACK_CLI_INTERNAL_H
#define KNAPSACK_CLI_INTERNAL_H

#include "knapsack/knapsack.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Buffer-size constants. Defined as enums (true integral constant
 * expressions) so the static_asserts below actually fire and so that
 * preprocessor-style checks remain possible.
 */
enum {
  KNAPSACK_CLI_CAP_LINE_MAX = 256,
  KNAPSACK_CLI_ITEM_LINE_MAX = 8192,
  KNAPSACK_CLI_INITIAL_ITEM_CAP = 16
};

/* JSON-friendly name of a status code, for example "OK", "INVALID_ITEMS". */
const char *cli_status_to_string(knapsack_status_t status);

/* Append a JSON-escaped string into the given stream (no surrounding quotes). */
void cli_json_quote(FILE *stream, const char *text);

/* Parse the first line (capacity). Returns 0 on success, -1 on any error.
 * On success *capacity is set; on failure it is unspecified. */
int cli_parse_capacity(FILE *file, int *capacity);

/* Parse the second line (whitespace/comma-separated weight:value pairs).
 * On success returns 0, sets *items (allocated via malloc, caller frees) and
 * *count. On failure returns -1 and *items is NULL. */
int cli_parse_items(FILE *file, knapsack_item_t **items, size_t *count);

/* Parse a single buffer (e.g. for fuzzing). buffer need not be NUL terminated;
 * length bytes are consumed. Returns 0 on success and sets items and count
 * outputs. Caller must free the items array. */
int cli_parse_buffer(const char *buffer, size_t length, int *capacity, knapsack_item_t **items,
                     size_t *count);

/* Output helpers; all write to stdout. */
void cli_print_result_text(const knapsack_result_t *result);
void cli_print_result_json(const knapsack_result_t *result);
void cli_print_error_text(FILE *stream, const char *message);
void cli_print_error_json(FILE *stream, const char *message, knapsack_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* KNAPSACK_CLI_INTERNAL_H */
