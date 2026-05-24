#ifndef SCANNER_H_
#define SCANNER_H_

/* ================================= STRING ================================= */

typedef struct {
	char   *ptr;
	size_t  len;
} String;

#define S(X) (String) { (X), (int) sizeof(X)-1 }

/* ================================== HTTP ================================== */

typedef enum {
	HTTPMETHOD_GET
} HTTPMethod;

typedef enum {
	HTTP_1_1,
} HTTPVersion;

typedef struct {
	String name;
	String value;
} Header;

#define URI_MAX_SIZE 2048
#define REQUEST_LINE_AND_HEADER_MAX_SIZE 8192

/* ================================ SCANNER ================================= */

typedef enum {
	SCAN_OK = 0,

	SCAN_ERR_UNKNOWN_HTTP_METHOD,
	SCAN_ERR_UNKNOWN_HTTP_VERSION,
} ScanResult;

typedef struct {
	size_t len;
	size_t cur;
	char *src;
} Scanner;

ScanResult consume_method(Scanner *s, HTTPMethod *method);
ScanResult consume_target(Scanner *s, String *x);
ScanResult consume_version(Scanner *s, HTTPVersion *version);

#endif // SCANNER_H_
