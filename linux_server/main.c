#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_LEVEL_FATAL   1
#define LOG_LEVEL_ERROR   2
#define LOG_LEVEL_WARN    3
#define LOG_LEVEL_INFO    4
#define LOG_LEVEL_DEBUG   5
#define LOG_LEVEL         LOG_LEVEL_INFO

#define LOG_FATAL (LOG_LEVEL >= LOG_LEVEL_FATAL)
#define LOG_ERROR (LOG_LEVEL >= LOG_LEVEL_ERROR)
#define LOG_WARN  (LOG_LEVEL >= LOG_LEVEL_WARN)
#define LOG_INFO  (LOG_LEVEL >= LOG_LEVEL_INFO)
#define LOG_DEBUG (LOG_LEVEL >= LOG_LEVEL_DEBUG)

#define DLB_UNUSED(x) ((void)x)
#define DLB_ERROR(msg) do { dlb_assert(errno, msg, __FILE__, __LINE__); } while(0)
#define DLB_ASSERT(cond) do { if (!cond) dlb_assert(errno, #cond, __FILE__, __LINE__); } while(0)
#define DLB_ZERO(var) (memset(&var, 0, sizeof(var)))

#define GEN_LIST(e, ...) e,
#define GEN_STRING(e, ...) #e,

void dlb_assert(int err, const char* msg, const char *file, int line)
{
	fprintf(stderr, "[ERROR] [errno=%d, errstr=%s] at %s:%d, %s\n",
			err, strerror(errno), file, line, msg);
    fflush(stderr);
	exit(1);
}

#define CFG_FILENAME "../../server.cfg"
#define CFG_VAR_MAX_LEN 32
#define MAX_CONNECTIONS 5
#define PORT 8080

#define CONFIG_VARS(f)  \
    f(CFG_USERNAME)     \
    f(CFG_PASSWORD)     \

enum config_var
{
    CONFIG_VARS(GEN_LIST)
    CFG_COUNT
};
char *config_var_str[] = {
    CONFIG_VARS(GEN_STRING)
};

char config_vars[CFG_COUNT][CFG_VAR_MAX_LEN];

void gatekeeper(int sock_fd_server);
void copycat(int sock_fd, const char *client_ip);

void buffer_cpy(char *dst, char *src, int len)
{
    int i = 0;
    while (i < len)
    {
        dst[i] = src[i];
        i++;
    }
}

char read_byte(FILE *file, char *chr)
{
    char c = 0;
    if (fread(&c, sizeof(c), 1, file) != 1 && !feof(file))
    {
        DLB_ERROR("[Server] Failed to read byte from file\n");
    }
    if (chr) *chr = c;
#if LOG_DEBUG
    //printf("[DEBUG][byte] %c\n", c);
#endif
    return c;
}

int skip_until(FILE *file, char delim)
{
    char c = 0;
    int i = 0;

#if LOG_DEBUG
    printf("[DEBUG] Skip until %d\n", delim);
#endif

    for(;;)
    {
        read_byte(file, &c);
        i++;
        if (c == 0 || c == delim)
            break;
    }

#if LOG_DEBUG
    printf("[DEBUG] Skipped %d bytes\n", i);
#endif

    return i;
}

int read_until(FILE *file, char *buffer, int length, char delim)
{
    int i = 0;
    char c = 0;

#if LOG_DEBUG
    printf("[DEBUG] Read until %d\n", delim);
#endif

    while (i < length - 1)
    {
        read_byte(file, &c);
        if (c == delim)
            break;

        buffer[i] = c;
        i++;
    }

#if LOG_DEBUG
    printf("[DEBUG] Read %d bytes\n", i);
#endif

    if (c != delim)
    {
        skip_until(file, delim);
    }
    return i;
}

void load_config(const char *filename)
{
    FILE *file;
    enum config_var var;
#if LOG_INFO
    int i;
    char c;
#endif

    file = fopen(filename, "r");
    if (!file)
    {
        DLB_ERROR("[Server] Failed to open config file");
    }

    var = (enum config_var)0;
    for (; var < CFG_COUNT; var++)
    {
        read_until(file, config_vars[var], CFG_VAR_MAX_LEN, '\n');
    }

#if LOG_INFO
    printf("[Server] Loaded config:\n");
    var = (enum config_var)0;
    for (; var < CFG_COUNT; var++)
    {
        i = 0;
        printf("  [%d][%s] ", var, config_var_str[var]);
        for (; i < CFG_VAR_MAX_LEN; i++)
        {
            c = config_vars[var][i];
            if (c)
            {
                if (var == CFG_PASSWORD)
                {
                    printf("*");
                }
                else
                {
                    printf("%c", c);
                }
            }
            else
            {
                printf(".");
            }
        }
        printf("\n");
    }
#endif
}

int main(int argc, char *argv[])
{
    int sock_fd_server;
	int port;
	struct sockaddr_in serv_addr;
    int enable = 1;

    DLB_UNUSED(argc);
    DLB_UNUSED(argv);
	DLB_ZERO(serv_addr);

    load_config(CFG_FILENAME);

	printf("[Server] Creating socket\n");
	sock_fd_server = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd_server < 0)
	{
		DLB_ERROR("[Server] Failed to open socket");
	}

    if (setsockopt(sock_fd_server, SOL_SOCKET, SO_REUSEADDR, &enable,
                   sizeof(int)) < 0)
    {
        fprintf(stderr, "[ERROR][Server] Failed to set REUSEADDR socket options\n");
    }

	printf("[Server] Binding to port %d\n", PORT);
	port = PORT;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	if (bind(sock_fd_server, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		DLB_ERROR("[Server] Failed to bind to port");
	}

	printf("[Server] Listening on port %d\n", PORT);
	listen(sock_fd_server, MAX_CONNECTIONS);

	// Split gatekeeper into its own thread
	if (!fork())
	{
		gatekeeper(sock_fd_server);
    }
    else
    {
        // Run server until admin presses a key
        printf("[Server] Press any key to exit...\n");
        getchar();
        //close(sock_fd_server);
        shutdown(sock_fd_server, 2);
        printf("[Server] Goodbye\n");
	}

	return 0;
}

void gatekeeper(int sock_fd_server)
{
	struct sockaddr_in client_addr;
    int sock_fd_client;
	unsigned int client_len = sizeof(client_addr);
    const char *client_ip;
    int loop_hack = 0;

    // HACK: GateKeeper kept going home early, so @Dizenth suggested I
    //       increase his pay. It worked! DO NOT EDIT!!!!!!!!!!
    const int GateKeeperPay = 500000;

    DLB_UNUSED(GateKeeperPay);
	DLB_ZERO(client_addr);

    printf("[GateKeeper] on duty!\n");
	while (getppid() > 1 && loop_hack++ < 10)
	{
        sock_fd_client = accept(sock_fd_server,
                (struct sockaddr *)&client_addr, &client_len);
        if (sock_fd_client < 0)
        {
            // Parent died, time to leave!
            if (getppid() <= 1)
                break;

            // TODO: What else can I log here?
            //DLB_ERROR("Failed to accept incoming connection");
            fprintf(stderr, "[GateKeeper] Failed to accept incoming connection\n");
        }
        else
        {
            client_ip = inet_ntoa(client_addr.sin_addr);

            // Send client to copycat
            if (!fork())
            {
                copycat(sock_fd_client, client_ip);
                return;
            }
        }
        printf("[GateKeeper] loop_hack = %d\n", loop_hack);
	}
    printf("[GateKeeper] went home :(\n");
}

void copycat(int sock_fd, const char *client_ip)
{
	char buffer_in[256];
	char buffer_out[256];
	int bytes;
    int loop_hack = 0;

	DLB_ZERO(buffer_in);
	DLB_ZERO(buffer_out);

    printf("[CopyCat][%d][%s] Client connected\n", sock_fd, client_ip);
	while (getppid() > 1 && loop_hack++ < 50000)
	{
		// TODO: Loop until newline
		bytes = read(sock_fd, buffer_in, sizeof(buffer_in) - 1);
		if (bytes == 0)
		{
			break;
		}
		else if (bytes < 0)
		{
			DLB_ERROR("[CopyCat] Failed to read from socket");
		}
		buffer_in[bytes] = 0;
		printf("[CopyCat][%d][%s][RECV][%d bytes] %s\n", sock_fd, client_ip,
                bytes, buffer_in);

		//strcpy(buffer_out, "This is a response.");
		strcpy(buffer_out, buffer_in);
		//bytes = write(sock_fd, buffer_out, strlen(buffer_out));
		bytes = write(sock_fd, buffer_in, strlen(buffer_out));
		if (bytes == 0)
		{
			break;
		}
		else if (bytes < 0)
		{
			DLB_ERROR("[CopyCat] Failed to write to socket");
		}
		printf("[CopyCat][%d][%s][SEND][%d bytes] %s\n", sock_fd, client_ip,
                bytes, buffer_out);
	}
    printf("[CopyCat][%d][%s] Client disconnected\n", sock_fd, client_ip);
}
