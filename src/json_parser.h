#ifndef FOLDEN_JSON_PARSER_H
#define FOLDEN_JSON_PARSER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  FOLDEN_JSON_NULL,
  FOLDEN_JSON_BOOL,
  FOLDEN_JSON_NUMBER,
  FOLDEN_JSON_STRING,
  FOLDEN_JSON_ARRAY,
  FOLDEN_JSON_OBJECT
} folden_json_type_t;

typedef struct folden_json_node folden_json_node_t;

struct folden_json_node {
  folden_json_type_t type;
  char *key;
  
  union {
    int boolean;
    double number;
    char *string;
    struct {
      folden_json_node_t **items;
      size_t count;
      size_t capacity;
    } array;
    struct {
      folden_json_node_t **pairs;
      size_t count;
      size_t capacity;
    } object;
  } value;
};

/* Parses a JSON string into a DOM tree. The returned node must be freed with folden_json_free. */
folden_json_node_t *folden_json_parse(const char *json_str, size_t length);

/* Frees a JSON node and all its children */
void folden_json_free(folden_json_node_t *node);

/* Helper to get an object property by key. Returns NULL if not found or not an object. */
folden_json_node_t *folden_json_object_get(const folden_json_node_t *node, const char *key);

/* Helper to get array item by index. Returns NULL if out of bounds or not an array. */
folden_json_node_t *folden_json_array_get(const folden_json_node_t *node, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* FOLDEN_JSON_PARSER_H */
