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

/* =============================== CONSTANTS ================================ */

#define DEFAULT_PORT "8080"
#define BACKLOG 10

/* ================================= BUFFER ================================= */

struct buf {
	char *str;
	size_t size;
};

#define BUF_INIT { 0 , 0 }

int append_buf(struct buf *b, char *str, size_t size)
{
	char *tmp = realloc(b->str, b->size + size);
	if (NULL == tmp) {
		fprintf(stderr, "Memory allocation for buffer failed\n");
		return -1;
	}

	strncpy(tmp + b->size, str, size);
	b->str = tmp;
	b->size += size;

	return 1;
}

void reset_buf(struct buf *b)
{
	free(b->str);
	b->str = NULL;
	b->size = 0;
}

/* =========================== RECV & SEND + BUF ============================ */

#define CHUNK_SIZE 1 << 11

int recv_all_to_buf(const int fd, struct buf *b)
{
	ssize_t nrecv;
	char chunk[CHUNK_SIZE];

	while (true) {
		nrecv = recv(fd, chunk, sizeof(chunk), 0);
		if (-1 == nrecv) {
			perror("recv");
			return -1;
		}

		append_buf(b, chunk, (size_t)nrecv);

		if (CHUNK_SIZE > nrecv)
			break;
	}

	return 0;
}

int send_all_from_buf(const int fd, struct buf *b)
{
	size_t total_sent = 0;
	ssize_t nsend;
	size_t to_send;
	size_t remaining;

	while (total_sent < b->size) {
		remaining = b->size - total_sent;
		to_send = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;

		nsend = send(fd, b->str + total_sent, to_send, 0);
		if (-1 == nsend) {
			perror("send");
			return -1;
		}

		total_sent += (size_t)nsend;
	}

	return 0;
}

/* =========================== ACCEPT AND HANDLE ============================ */

void accept_and_handle_loop(const int sock_fd)
{
	int new_fd;
	struct sockaddr_storage their_addr;
	socklen_t sin_size = sizeof(their_addr);
	struct buf b = BUF_INIT;

	while (true) {
		new_fd = accept(sock_fd, (struct sockaddr *)&their_addr, &sin_size);
		recv_all_to_buf(new_fd, &b);
		printf("Received: %s (%ld bytes)\n", b.str, b.size);
		send_all_from_buf(new_fd, &b);
		reset_buf(&b);
		close(new_fd);
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
