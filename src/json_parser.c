#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

typedef struct {
  const char *json;
  size_t length;
  size_t pos;
} parser_ctx_t;

static folden_json_node_t *parse_value(parser_ctx_t *ctx);

static void skip_whitespace(parser_ctx_t *ctx) {
  while (ctx->pos < ctx->length && isspace((unsigned char)ctx->json[ctx->pos])) {
    ctx->pos++;
  }
}

static folden_json_node_t *create_node(folden_json_type_t type) {
  folden_json_node_t *n = calloc(1, sizeof(folden_json_node_t));
  if (n) n->type = type;
  return n;
}

void folden_json_free(folden_json_node_t *node) {
  if (!node) return;
  free(node->key);
  switch (node->type) {
    case FOLDEN_JSON_STRING:
      free(node->value.string);
      break;
    case FOLDEN_JSON_ARRAY:
      for (size_t i = 0; i < node->value.array.count; i++) {
        folden_json_free(node->value.array.items[i]);
      }
      free(node->value.array.items);
      break;
    case FOLDEN_JSON_OBJECT:
      for (size_t i = 0; i < node->value.object.count; i++) {
        folden_json_free(node->value.object.pairs[i]);
      }
      free(node->value.object.pairs);
      break;
    default:
      break;
  }
  free(node);
}

static char *parse_string_val(parser_ctx_t *ctx) {
  if (ctx->pos >= ctx->length || ctx->json[ctx->pos] != '"') return NULL;
  ctx->pos++; // skip "
  
  size_t start = ctx->pos;
  while (ctx->pos < ctx->length && ctx->json[ctx->pos] != '"') {
    if (ctx->json[ctx->pos] == '\\') ctx->pos++; // skip escaped
    ctx->pos++;
  }
  
  if (ctx->pos >= ctx->length) return NULL; // unclosed string
  
  size_t len = ctx->pos - start;
  char *str = malloc(len + 1);
  if (!str) return NULL;
  
  // Naive unescaping for simplicity
  size_t dst = 0;
  for (size_t src = start; src < ctx->pos; src++) {
    if (ctx->json[src] == '\\' && src + 1 < ctx->pos) {
      src++;
      switch (ctx->json[src]) {
        case 'n': str[dst++] = '\n'; break;
        case 'r': str[dst++] = '\r'; break;
        case 't': str[dst++] = '\t'; break;
        case '"': str[dst++] = '"'; break;
        case '\\': str[dst++] = '\\'; break;
        default: str[dst++] = ctx->json[src]; break;
      }
    } else {
      str[dst++] = ctx->json[src];
    }
  }
  str[dst] = '\0';
  ctx->pos++; // skip closing "
  return str;
}

static folden_json_node_t *parse_string(parser_ctx_t *ctx) {
  char *str = parse_string_val(ctx);
  if (!str) return NULL;
  folden_json_node_t *node = create_node(FOLDEN_JSON_STRING);
  if (!node) {
    free(str);
    return NULL;
  }
  node->value.string = str;
  return node;
}

static folden_json_node_t *parse_number(parser_ctx_t *ctx) {
  size_t start = ctx->pos;
  while (ctx->pos < ctx->length && (isdigit((unsigned char)ctx->json[ctx->pos]) || 
         ctx->json[ctx->pos] == '-' || ctx->json[ctx->pos] == '.' || 
         ctx->json[ctx->pos] == 'e' || ctx->json[ctx->pos] == 'E' || 
         ctx->json[ctx->pos] == '+')) {
    ctx->pos++;
  }
  
  size_t len = ctx->pos - start;
  if (len == 0) return NULL;
  
  char buf[64];
  if (len >= sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, ctx->json + start, len);
  buf[len] = '\0';
  
  folden_json_node_t *node = create_node(FOLDEN_JSON_NUMBER);
  if (!node) return NULL;
  node->value.number = strtod(buf, NULL);
  return node;
}

static folden_json_node_t *parse_object(parser_ctx_t *ctx) {
  ctx->pos++; // skip {
  skip_whitespace(ctx);
  
  folden_json_node_t *node = create_node(FOLDEN_JSON_OBJECT);
  if (!node) return NULL;
  
  if (ctx->pos < ctx->length && ctx->json[ctx->pos] == '}') {
    ctx->pos++;
    return node;
  }
  
  while (ctx->pos < ctx->length) {
    skip_whitespace(ctx);
    char *key = parse_string_val(ctx);
    if (!key) goto error;
    
    skip_whitespace(ctx);
    if (ctx->pos >= ctx->length || ctx->json[ctx->pos] != ':') {
      free(key);
      goto error;
    }
    ctx->pos++; // skip :
    
    skip_whitespace(ctx);
    folden_json_node_t *val = parse_value(ctx);
    if (!val) {
      free(key);
      goto error;
    }
    val->key = key;
    
    if (node->value.object.count == node->value.object.capacity) {
      size_t new_cap = node->value.object.capacity == 0 ? 4 : node->value.object.capacity * 2;
      void *p = realloc(node->value.object.pairs, new_cap * sizeof(folden_json_node_t *));
      if (!p) {
        folden_json_free(val);
        goto error;
      }
      node->value.object.pairs = p;
      node->value.object.capacity = new_cap;
    }
    node->value.object.pairs[node->value.object.count++] = val;
    
    skip_whitespace(ctx);
    if (ctx->pos < ctx->length && ctx->json[ctx->pos] == ',') {
      ctx->pos++;
      continue;
    } else if (ctx->pos < ctx->length && ctx->json[ctx->pos] == '}') {
      ctx->pos++;
      return node;
    } else {
      goto error;
    }
  }
  
error:
  folden_json_free(node);
  return NULL;
}

static folden_json_node_t *parse_array(parser_ctx_t *ctx) {
  ctx->pos++; // skip [
  skip_whitespace(ctx);
  
  folden_json_node_t *node = create_node(FOLDEN_JSON_ARRAY);
  if (!node) return NULL;
  
  if (ctx->pos < ctx->length && ctx->json[ctx->pos] == ']') {
    ctx->pos++;
    return node;
  }
  
  while (ctx->pos < ctx->length) {
    skip_whitespace(ctx);
    folden_json_node_t *val = parse_value(ctx);
    if (!val) goto error;
    
    if (node->value.array.count == node->value.array.capacity) {
      size_t new_cap = node->value.array.capacity == 0 ? 4 : node->value.array.capacity * 2;
      void *p = realloc(node->value.array.items, new_cap * sizeof(folden_json_node_t *));
      if (!p) {
        folden_json_free(val);
        goto error;
      }
      node->value.array.items = p;
      node->value.array.capacity = new_cap;
    }
    node->value.array.items[node->value.array.count++] = val;
    
    skip_whitespace(ctx);
    if (ctx->pos < ctx->length && ctx->json[ctx->pos] == ',') {
      ctx->pos++;
      continue;
    } else if (ctx->pos < ctx->length && ctx->json[ctx->pos] == ']') {
      ctx->pos++;
      return node;
    } else {
      goto error;
    }
  }
  
error:
  folden_json_free(node);
  return NULL;
}

static folden_json_node_t *parse_value(parser_ctx_t *ctx) {
  skip_whitespace(ctx);
  if (ctx->pos >= ctx->length) return NULL;
  
  char c = ctx->json[ctx->pos];
  if (c == '"') return parse_string(ctx);
  if (c == '{') return parse_object(ctx);
  if (c == '[') return parse_array(ctx);
  if (c == '-' || isdigit((unsigned char)c)) return parse_number(ctx);
  
  if (ctx->pos + 4 <= ctx->length && strncmp(ctx->json + ctx->pos, "true", 4) == 0) {
    ctx->pos += 4;
    folden_json_node_t *node = create_node(FOLDEN_JSON_BOOL);
    if (node) node->value.boolean = 1;
    return node;
  }
  if (ctx->pos + 5 <= ctx->length && strncmp(ctx->json + ctx->pos, "false", 5) == 0) {
    ctx->pos += 5;
    folden_json_node_t *node = create_node(FOLDEN_JSON_BOOL);
    if (node) node->value.boolean = 0;
    return node;
  }
  if (ctx->pos + 4 <= ctx->length && strncmp(ctx->json + ctx->pos, "null", 4) == 0) {
    ctx->pos += 4;
    return create_node(FOLDEN_JSON_NULL);
  }
  
  return NULL;
}

folden_json_node_t *folden_json_parse(const char *json_str, size_t length) {
  if (!json_str || length == 0) return NULL;
  
  parser_ctx_t ctx = { .json = json_str, .length = length, .pos = 0 };
  return parse_value(&ctx);
}

folden_json_node_t *folden_json_object_get(const folden_json_node_t *node, const char *key) {
  if (!node || node->type != FOLDEN_JSON_OBJECT || !key) return NULL;
  for (size_t i = 0; i < node->value.object.count; i++) {
    if (node->value.object.pairs[i]->key && strcmp(node->value.object.pairs[i]->key, key) == 0) {
      return node->value.object.pairs[i];
    }
  }
  return NULL;
}

folden_json_node_t *folden_json_array_get(const folden_json_node_t *node, size_t index) {
  if (!node || node->type != FOLDEN_JSON_ARRAY || index >= node->value.array.count) return NULL;
  return node->value.array.items[index];
}
