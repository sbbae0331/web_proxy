#include <stdio.h> // for perror
#include <string.h> // for memset
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket
#include <stdlib.h>
#include <pthread.h>
#include <netdb.h> // for struct hostent, gethostbyname

const static int BUFSIZE = 2048;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct Args
{
	int childfd;
	int pts_sockfd;
};

void *client_to_proxy(void *data);
void *server_to_proxy(void *data);

int main(int argc, char *argv[]) 
{
	if (argc != 2) {
		printf("syntax : web_proxy <tcp port>\n");
		printf("sample : web_proxy 8080\n");
		return -1;
	}

	int PORT = atoi(argv[1]);


	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2);
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

	while (true) 
	{
		struct sockaddr_in addr;
		socklen_t clientlen = sizeof(sockaddr);
		int childfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
		if (childfd < 0) {
			perror("ERROR on accept");
			break;
		}

		ssize_t sent;
		char *host;

		int pts_sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (pts_sockfd == -1) {
			perror("socket failed");
		}

		int optval = 1;
		setsockopt(pts_sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

		char buf[BUFSIZE];

		ssize_t received = recv(childfd, buf, BUFSIZE - 1, 0);

		buf[received] = '\0';
		printf("%s\n", buf);
		
		char *ptr;
		char tbuf[2048];
		memcpy(tbuf, buf, strlen(buf));
		ptr = strtok(tbuf, "\r\n");
		ptr = strtok(NULL, "\r\n");
		ptr = strtok(ptr, ": ");
		host = strtok(NULL, ": ");
		printf("%s\n", host);

		struct hostent *server;
		if((server = gethostbyname(host)) == NULL) {
			perror("gethostbyname error");
			return -1;
		}

		//struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(80);
		memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
		//memset(addr.sin_zero, 0, sizeof(addr.sin_zero));	

		if(connect(pts_sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("connect failed");
		}	

		printf("%s\n", buf);

		sent = send(pts_sockfd, buf ,strlen(buf), 0);
		if (sent == 0) {
			perror("send failed");
			break;
		}	

		int status;
		pthread_t pid[2];

		Args *args;
		args = (Args *)malloc(sizeof(Args));
		args->childfd = childfd;
		args->pts_sockfd = pts_sockfd;

		pthread_create(&pid[0], NULL, client_to_proxy, (void *)args);
		pthread_create(&pid[1], NULL, server_to_proxy, (void *)args);

		pthread_join(pid[0], (void **)&status);
		pthread_join(pid[1], (void **)&status);

		close(childfd);
	}

	close(sockfd);
}

void *client_to_proxy(void *data)
{
	Args *args = (Args *)data;
	int childfd = args->childfd;
	int pts_sockfd = args->pts_sockfd;
	ssize_t sent;
	
	// proxy to server

	while (true)
	{
		char buf[BUFSIZE];

		ssize_t received = recv(childfd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			break;
		}
		buf[received] = '\0';
		printf("%s\n", buf);

		sent = send(pts_sockfd, buf ,strlen(buf), 0);
		if (sent == 0) {
			perror("send failed");
			break;
		}
	}
}

void *server_to_proxy(void *data)
{
	Args *args = (Args *)data;
	int childfd = args->childfd;
	int pts_sockfd = args->pts_sockfd;
	ssize_t sent;

	while (true)
	{
		char buf[BUFSIZE];

		ssize_t received = recv(pts_sockfd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			break;
		}
		buf[received] = '\0';

		printf("Received\n");
		printf("%s\n", buf);

		ssize_t sent = send(childfd, buf, strlen(buf), 0);
		if (sent == 0) {
			perror("send failed");
			break;
		}
	}
}

