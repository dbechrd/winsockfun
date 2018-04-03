#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DLB_UNUSED(x) ((void)x)
#define DLB_ERROR(msg) do { dlb_assert(errno, msg, __FILE__, __LINE__); } while(0)
#define DLB_ASSERT(cond) do { if (!cond) dlb_assert(errno, #cond, __FILE__, __LINE__); } while(0)
#define DLB_ZERO(var) (memset(&var, 0, sizeof(var)))

void dlb_assert(int err, const char* msg, const char *file, int line)
{
	fprintf(stderr, "[ERROR] [errno=%d, errstr=%s] at %s:%d, %s\n",
			err, strerror(errno), file, line, msg);
	exit(1);
}

// TODO: Config file
#define MAX_CONNECTIONS 5
#define PORT 8080
int quit;
int sock_fd_server;
int sock_fd_clients[MAX_CONNECTIONS];

void gatekeeper();
void copycat(int sock_fd, const char *client_ip);

int main(int argc, char *argv[])
{
	int port;
	struct sockaddr_in serv_addr;
	int client_idx;

    DLB_UNUSED(argc);
    DLB_UNUSED(argv);
	DLB_ZERO(serv_addr);

	printf("Creating socket\n");
	sock_fd_server = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd_server < 0)
	{
		DLB_ERROR("Failed to open socket");
	}

	printf("Binding to port %d\n", PORT);
	port = PORT;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	if (bind(sock_fd_server, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		DLB_ERROR("Failed to bind to port");
	}

	printf("Listening on port %d\n", PORT);
	listen(sock_fd_server, MAX_CONNECTIONS);

	// Split gatekeeper into its own thread
	if (!fork())
	{
		gatekeeper();
	}

	// Run server until admin presses a key
	getchar();
    for (client_idx = 0; client_idx < MAX_CONNECTIONS; client_idx++)
    {
        if (sock_fd_clients[client_idx])
        {
            close(sock_fd_clients[client_idx]);
        }
    }
	close(sock_fd_server);
	//shutdown(sock_fd_server, 2);
	printf("Goodbye\n");
    quit = 1;
	return 0;
}

void gatekeeper()
{
	struct sockaddr_in client_addr;
	unsigned int client_len = sizeof(client_addr);
    int client_idx;
    const char *client_ip;

	DLB_ZERO(client_addr);

	while (!quit)
	{
        // Find available socket
        for (client_idx = 0; client_idx < MAX_CONNECTIONS; client_idx++)
        {
            if (sock_fd_clients[client_idx] == 0)
                break;
        }

        if (client_idx < MAX_CONNECTIONS)
        {
            sock_fd_clients[client_idx] = accept(sock_fd_server,
                    (struct sockaddr *)&client_addr, &client_len);
            if (sock_fd_clients[client_idx] < 0)
            {
                // TODO: What else can I log here?
                fprintf(stderr, "Failed to accept incoming connection\n");
                //DLB_ERROR("Failed to accept incoming connection");
            }

            client_ip = inet_ntoa(client_addr.sin_addr);

            // Send client to copycat
            if (!fork())
            {
                copycat(sock_fd_clients[client_idx], client_ip);
                break;
            }
        }
        else
        {
            printf("Connection refused, too many clients\n");
        }
	}
}

void copycat(int sock_fd, const char *client_ip)
{
	char buffer_in[256];
	char buffer_out[256];
	int bytes;

	DLB_ZERO(buffer_in);
	DLB_ZERO(buffer_out);

    printf("[HELO] Client connected: %s\n", client_ip);

	while (!quit)
	{
		// TODO: Loop until newline
		bytes = read(sock_fd, buffer_in, sizeof(buffer_in) - 1);
		if (bytes == 0)
		{
			break;
		}
		else if (bytes < 0)
		{
			DLB_ERROR("Failed to read from socket");
		}
		buffer_in[bytes] = 0;
		printf("[RECV][%d bytes]:\n%s\n", bytes, buffer_in);

		//strcpy(buffer_out, "This is a response.");
		strcpy(buffer_out, buffer_in);
		//bytes = write(client_sock_fd, buffer_out, strlen(buffer_out));
		bytes = write(sock_fd, buffer_in, strlen(buffer_out));
		if (bytes == 0)
		{
			break;
		}
		else if (bytes < 0)
		{
			DLB_ERROR("Failed to write to socket");
		}
		printf("[SEND][%d bytes]:\n%s\n", bytes, buffer_out);
	}

    printf("[GBYE] Client disconnected: %s\n", client_ip);
}
