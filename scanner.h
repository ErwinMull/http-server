#ifndef SCANNER_H_
#define SCANNER_H_

/* =============================== CONSTANTS ================================ */

#define URI_MAX_SIZE 2048
#define REQUEST_LINE_AND_HEADER_MAX_SIZE 8192

/* ================================= ENUMS ================================== */

typedef enum {
	HTTPMETHOD_INVALID = 0,
	HTTPMETHOD_GET
} HTTPMethod;

typedef enum {
	VALID_TARGET = 0,
	TARGET_TOO_LONG,
	MALFORMED_TARGET,
	MALICIOUS_TARGET
} HTTPTargetOutcome;

typedef enum {
	INVALID_HTTPVERSION = 0,
	HTTP_1_1,
} HTTPVersion;

/* ================================ STRUCTS ================================= */

typedef struct {
	char   *ptr;
	size_t  len;
} String;

#define INIT_STRING { NULL, 0 }

typedef struct {
	char   *src;
	size_t  len;
	size_t  cur;
} Scanner;

#define INIT_SCANNER { NULL, 0, 0 }

typedef struct {
	String name;
	String value;
} Header;

#define INIT_HEADER { INIT_STRING, INIT_STRING }

/* =============================== PROCEDURES =============================== */

HTTPMethod scan_method(Scanner *s);
HTTPTargetOutcome scan_target(Scanner *s, String *x);
HTTPVersion scan_version(Scanner *s);

#endif // SCANNER_H_
