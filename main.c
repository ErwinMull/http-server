/* ================================ INCLUDES ================================ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/ioctl.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "scanner.h"

/* ================================= CONFIG ================================= */

#define DEFAULT_PORT "8080"
#define BACKLOG 10
#define CHUNK_SIZE 1024
#define BUFFER_MIN_CAP 1024

/* ============================== RESULT CODES ============================== */

typedef enum {
	RES_OK = 0,

	RES_ERR_MEMORY,
	RES_ERR_IO,
	RES_ERR_OVERFLOW
} Result;

/* ================================= BUFFER ================================= */

typedef struct {
	size_t len;
	size_t cap;
	char *data;
} Buffer;

Result buf_append(Buffer *b, const char *str, size_t size)
{
	size_t required = b->len + size;

	if (SIZE_MAX < required) {
		return RES_ERR_OVERFLOW;
	}

	if (b->len + size > b->cap) {
		size_t new_capacity = b->cap ? b->cap : BUFFER_MIN_CAP;
		while (new_capacity < required) {
			new_capacity *= 2;
		}

		char *tmp = realloc(b->data, new_capacity);
		if (!tmp) {
			return RES_ERR_MEMORY;
		}
		b->data = tmp;
		b->cap = new_capacity;
	}

	memcpy(b->data + b->len, str, size);
	b->len += size;

	return RES_OK;
}

void buf_free(Buffer *b)
{
	free(b->data);
	b->data = NULL;
	b->len = 0;
	b->cap = 0;
}

/* =============================== SOCKET I/O =============================== */

Result recv_all_to_buf(const int fd, Buffer *b)
{
	ssize_t nrecv;
	char chunk[CHUNK_SIZE];
	Result r;

	while (true) {
		nrecv = recv(fd, chunk, sizeof(chunk), 0);
		if (-1 == nrecv) {
			return RES_ERR_IO;
		}

		r = buf_append(b, chunk, (size_t)nrecv);
		if (RES_OK != r) {
			return r;
		}

		if (CHUNK_SIZE > nrecv) {
			break;
		}
	}

	return RES_OK;
}

Result send_all_from_buf(const int fd, Buffer *b)
{
	size_t total_sent = 0;
	ssize_t nsend;
	size_t to_send;
	size_t remaining;

	while (total_sent < b->len) {
		remaining = b->len - total_sent;
		to_send = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;

		nsend = send(fd, b->data + total_sent, to_send, 0);
		if (-1 == nsend) {
			return RES_ERR_IO;
		}

		total_sent += (size_t)nsend;
	}

	return RES_OK;
}

/* ============================= FILE HANDLING ============================== */

Result read_file_contents_to_buffer(const char *path, Buffer *b)
{
	FILE *fp = NULL;
	size_t len = 0;
	char chunk[CHUNK_SIZE];
	size_t nread = 0;
	size_t total_read = 0;
	Result status;

	fp = fopen(path, "r");
	if (NULL == fp) {
		return RES_ERR_IO;
	}

	fseek(fp, 0L, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	while (total_read < len) {
		nread = fread(chunk, sizeof(char), CHUNK_SIZE, fp);
		total_read += nread;

		if (0 == nread && total_read != len)
			return RES_ERR_IO;

		status = buf_append(b, chunk, nread);

		if (RES_OK != status)
			return status;
	}

	fclose(fp);

	return RES_OK;
}

/* ============================= HANDLE_METHODS ============================= */

int handle_get(Buffer *b, String *target)
{
	char line[] = "HTTP/1.1 200 OK\r\n\r\n";
	char *str = NULL;
	size_t str_len = 0;
	size_t offset = 0;

	buf_append(b, line, strlen(line));
	if (1 == target->len && '/' == target->ptr[0]) {
		read_file_contents_to_buffer("index.html", b);
	} else {
		str_len = target->len + 1;
		if ('/' == target->ptr[0]) {
			offset = 1;
			str_len -= 1;
		}
		str = malloc(str_len);
		if (NULL == str) {
			return -1;
		}
		strncpy(str, target->ptr + offset, str_len - 1);
		str[str_len - 1] = '\0';
		read_file_contents_to_buffer(str, b);
		free(str);
	}

	return 0;
}

int handle_client(const int client_fd)
{
	Scanner s = { 0 };
	String target = { 0 };
	Buffer b_in = { 0 };
	Buffer b_out = { 0 };

	HTTPMethod method;
	ScanResult r;

	recv_all_to_buf(client_fd, &b_in);

	s.src = b_in.data;
	s.len = b_in.len;

	r = consume_method(&s, &method);
	if (SCAN_OK != r) {
		// TODO: HTTP ERROR
		return -1;
	}

	r = consume_target(&s, &target);
	if (SCAN_OK == r && HTTPMETHOD_GET == method) {
		handle_get(&b_out, &target);
	} else {
		// TODO: NOT IMPLEMENTED
		return -1;
	}

	send_all_from_buf(client_fd, &b_out);

	buf_free(&b_in);
	buf_free(&b_out);

	return 0;
}

/* =========================== ACCEPT AND HANDLE ============================ */

void accept_and_handle_loop(const int sock_fd)
{
	int client_fd;
	struct sockaddr_storage client_addr;
	socklen_t sin_size = sizeof(client_addr);

	while (true) {
		client_fd = accept(sock_fd,
				(struct sockaddr *)&client_addr,
				&sin_size);
		handle_client(client_fd);
		close(client_fd);
	}

	return;
}

/* ============================= SERVER STARTUP ============================= */

int setup_listener(const char *port)
{
	int status;
	int sock_fd;
	struct addrinfo hints;
	struct addrinfo *result, *rp;

	int yes = 1;

	/* Hints contain prefilled information */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;        /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;    /* Stream sockets (TCP) */
	hints.ai_flags = AI_PASSIVE;        /* Wildcard IP address */
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	/* Get a linked list of addrinfo structs */
	status = getaddrinfo(NULL, port, &hints, &result);
	if (0 != status) {
		fprintf(stderr, "gai error: %s\n", gai_strerror(status));
		return -1;
	}

	/* Create a socket and bind it to a port for the first working
	 * addrinfo */
	for(rp = result; NULL != rp; rp = rp->ai_next) {
		sock_fd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (-1 == sock_fd) {
			perror("socket");
			continue;
		}

		/* Set socket option to reuse port */
		if (-1 == setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
					&yes, sizeof(int))) {
			perror("setsockopt");
			return -1;
		}

		if (0 == bind (sock_fd, rp->ai_addr, rp->ai_addrlen)) {
			break; /* Success */
		} else {
			close(sock_fd);
			perror("bind");
			continue;
		}
	}

	freeaddrinfo(result);

	if (NULL == rp) {
		fprintf(stderr, "Failed to bind\n");
		return -1;
	}

	if (-1 == listen(sock_fd, BACKLOG)) {
		close(sock_fd);
		perror("listen");
		return -1;
	}

	return sock_fd;
}

/* ================================== MAIN ================================== */

/* TODO: port as argument -p */
/* IDEA: limit URI to stack to avoid stuf like '/../../file' */
/* BETTER IDEA: just validate URI via number: .. > -1; . > 0; else +1 */
/* TODO: buffer in own file */

int main (int argc, char **argv)
{
	int sock_fd;

	sock_fd = setup_listener(DEFAULT_PORT);

	if (-1 == sock_fd) {
		fprintf(stderr, "Server startup failed\n");
		exit(EXIT_FAILURE);
	}

	printf("Server startup\n");

	accept_and_handle_loop(sock_fd);

	close(sock_fd);

	printf("Server shutdown\n");

	return 0;
}
