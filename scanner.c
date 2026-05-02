/* ================================ INCLUDES ================================ */

#include <stdbool.h>
#include <string.h>

#include "scanner.h"

/* ================================= MACROS ================================= */

#define S(X) (String) { (X), (int) sizeof(X)-1 }

/* ========================== CHARACTER PREDICATES ========================== */

bool is_digit(char c)
{
	return c >= '0' && c <= '9';
}

bool is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_space(char c)
{
	return c == ' ';
}

bool is_tchar(char c)
{
	return c == '!' || c == '#' || c == '$' || c == '%' || c == '&'
		|| c == '\'' || c == '*' || c == '+' || c == '-' || c == '.'
		|| c == '^' || c == '_' || c == '`' || c == '|' || c == '~'
		|| is_digit(c) || is_alpha(c);
}

/* ================================ CONSUME ================================= */

bool consume_str(Scanner *s, String x)
{
	if (x.len == 0)
		return false;

	if (x.len > s->len - s->cur)
		return false;

	for (size_t i = 0; i < x.len; i++)
		if (s->src[s->cur+i] != x.ptr[i])
			return false;

	s->cur += x.len;
	return true;
}

bool consume_space(Scanner *s)
{
	while (s->len > s->cur && is_space(s->src[s->cur]))
		s->cur++;
	return true;
}

bool consume_crlf(Scanner *s)
{
	if (2 >= s->len - s->cur &&
		'\r' == s->src[s->cur] &&
		'\n' == s->src[s->cur + 1]) {
		return false;
	}

	s->cur += 2;
	return true;
}

/* ================================ PARSING ================================= */

HTTPMethod scan_method(Scanner *s)
{
	if (0 != s->cur) {
		// TODO: better way to handle call on non-zero index?
		return HTTPMETHOD_INVALID;
	} else if (consume_str(s, S("GET "))) {
		return HTTPMETHOD_GET;
	}

	return HTTPMETHOD_INVALID;
}

HTTPTargetOutcome scan_target(Scanner *s, String *x)
{
	size_t target_start = s->cur;

	while (s->len > s->cur && !is_space(s->src[s->cur])) {
		s->cur++;
	}

	x->ptr = s->src + target_start;
	x->len = s->cur - target_start;

	return VALID_TARGET;
}

HTTPVersion scan_version(Scanner *s)
{
	if (consume_str(s, S("HTTP/1.1"))) {
		return HTTP_1_1;
	}

	return INVALID_HTTPVERSION;
}
