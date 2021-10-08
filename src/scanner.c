#include <stdio.h>
#include <string.h>
#include "include/scanner.h"
#include "include/common.h"
typedef struct {
  const char* start;
  const char* current;
  int line;
} Scanner;

Scanner scanner;

void init_scanner(const char* src) {
  scanner.start = src;
  scanner.current = src;
  scanner.line = 1;
}

static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') ||
  (c >= 'A' && c <= 'Z') ||
  c == '_';
}

static bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static bool at_end() {
  return *scanner.current == '\0';
}

static char adv() {
  scanner.current++;
  return scanner.current[-1];
}

static char peek() {
  return *scanner.current;
}

static char peek_next() {
  if (at_end()) return '\0';
  return scanner.current[1];
}

static bool match(char expected) {
  if (at_end()) return false;

  if (*scanner.current != expected) return false;

  scanner.current++;

  return true;
}

static Token make_token(TokenType type) {
  Token token;

  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;

  return token;
}

static Token token_error(const char* message) {
  Token token;

  token.type = T_ERR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;

  return token;
}

static void skip_wspc() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        adv();
        break;
      case '\n':
        scanner.line++;
        adv();
        break;
      case '%':
        while (peek() != '\n' && !at_end()) adv();
        break;
      default:
        return;
    }
  }
}

static TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
  if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }
  return T_IDENT;
}

static TokenType ident_type() {
  switch (scanner.start[0]) {
    case 'a': return check_keyword(1, 2, "nd", T_AND);
    case 'c':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          // Funny ass joke.
          case 'l': return check_keyword(2, 3, "ass", T_CLASS);
          case 'a': return check_keyword(2, 2, "se", T_CASE);
        }
      }
    case 'd': return check_keyword(1, 1, "o", T_DO);
    case 'e':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'l': return check_keyword(2, 2, "se", T_ELSE);
          case 'n': return check_keyword(2, 1, "d", T_END);
        }
      }
      break;
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return check_keyword(2, 3, "lse", T_FALSE);
          case 'o': return check_keyword(2, 1, "r", T_FOR);
          case 'u': return check_keyword(2, 2, "nc", T_FUN);
        }
      }
      break;
    case 'i': return check_keyword(1, 1, "f", T_IF);
    case 'm': return check_keyword(1, 4, "atch", T_MATCH);
    case 'n': return check_keyword(1, 2, "il", T_NIL);
    case 'o': return check_keyword(1, 1, "r", T_OR);
    case 'p': return check_keyword(1, 3, "uts", T_PRINT);
    case 'r': return check_keyword(1, 5, "eturn", T_RETURN);
    case 's':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'u': return check_keyword(2, 3, "per", T_SUPER);
          case 'e': return check_keyword(2, 1, "t", T_VAR);
        }
      }
      break;
    case 't':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'h': return check_keyword(2, 2, "is", T_THIS);
          case 'r': return check_keyword(2, 2, "ue", T_TRUE);
        }
      }
      break;
    case 'w': return check_keyword(1, 4, "hile", T_WHILE);
  }
  return T_IDENT;
}

static Token ident() {
  while (is_alpha(peek()) || is_digit(peek())) adv();
  return make_token(ident_type());
}

static Token num() {
  while (is_digit(peek())) adv();

  if (peek() == '.' && is_digit(peek_next())) {
    adv();

    while (is_digit(peek())) adv();
  }
  return make_token(T_NUM);
}

static Token string() {
  while (peek() != '"' && !at_end()) {
    if (peek() == '\n') scanner.line++;
    adv();
  }
  if (at_end()) return token_error("Non-terminated string.");

  adv();

  return make_token(T_STRING);
}

Token scan_token() {
  skip_wspc();

  scanner.start = scanner.current;

  if (at_end()) return make_token(T_EOS);

  char c = adv();

  if (is_alpha(c)) return ident();
  if (is_digit(c)) return num();

  switch (c) {
    case '(': return make_token(T_LPAREN);
    case ')': return make_token(T_RPAREN);
    case '{': return make_token(T_LBRACE);
    case '}': return make_token(T_RBRACE);
    case '[': return make_token(T_LBRACK);
    case ']': return make_token(T_RBRACK);
    case ';': return make_token(T_SEMICOLON);
    case ':': return make_token(T_COLON);
    case ',': return make_token(T_COMMA);
    case '.': return make_token(T_DOT);
    case '+': return make_token(T_PLUS);
    case '/': return make_token(T_SLASH);
    case '*': return make_token(T_STAR);
    case '~': return make_token(T_TILDE);
    case '?': return make_token(T_QMARK);

    case '-':
      return make_token(match('>') ? T_RARROW : T_MINUS);
    case '!':
      return make_token(match('=') ? T_BANG_EQU : T_BANG);
    case '=':
      return make_token(match('=') ? T_EQU_EQU : T_EQU);
    case '<':
      if (match('=')) {
        return make_token(T_LESS_EQU);
      }
      else if (match('-')) {
        return make_token(T_LARROW);
      }
      else {
        return make_token(T_LESS);
      }
    case '>':
      return make_token(match('=') ? T_GREATER_EQU : T_GREATER);
    case '"': return string();
  }

  return token_error("Unexpected character.");
}
