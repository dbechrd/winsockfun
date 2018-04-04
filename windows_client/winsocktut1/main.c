////////////////////////////////////////////////////////////////////////////////
//      Author: Dan Bechard
//        Date: 2018-04-01
// Description: A really great WinSock application that does a lot of useful
//              stuff.
////////////////////////////////////////////////////////////////////////////////

#pragma comment(lib, "Ws2_32.lib")

#undef UNICODE
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <assert.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#define RESP_BUFFER_SIZE 128
#define RESP_MAX_LEN (RESP_BUFFER_SIZE - 1)

int msg_send(SOCKET s, const char *msg, int len);
int msg_recv(SOCKET s, char *buf, int *len);
int err(const char *msg);

int main(int argc, char *argv[])
{
    const char *HOSTNAME = "theprogrammingjunkie.com";
    const char *PORT = "8080";

    char ip[32] = { 0 };
    int i = 0;

    const char *message = 0;
    int response_len = 0;
    char response_buf[RESP_BUFFER_SIZE] = { 0 };
    
    WSADATA wsa = { 0 };
    SOCKET sock = { 0 };
    struct addrinfo hints = { 0 };
    struct addrinfo *addr_info = NULL;
    struct addrinfo *ipv4 = NULL;

    printf("[INIT] Let's get ready to rumble!\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
    {
        return err("Failed to initialize WinSock");
    }
    printf("[INIT] It works!\n");

    printf("[ADDR] Resolving '%s'\n", HOSTNAME);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(HOSTNAME, PORT, &hints, &addr_info))
    {
        return err("Failed to get address info");
    }

    for (ipv4 = addr_info; ipv4 != NULL; ipv4 = ipv4->ai_next)
    {
        if (ipv4->ai_family == AF_INET)
        {
            printf("       [%d] %s\n", i,
                   inet_ntoa(((struct sockaddr_in *)ipv4->ai_addr)->sin_addr));
            break;
        }
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        return err("Failed to create socket");
    }

    if (connect(sock, ipv4->ai_addr, ipv4->ai_addrlen) == SOCKET_ERROR)
    {
        WSAECONNREFUSED;
        return err("Failed to connect");
    }
    freeaddrinfo(addr_info);

    message = "Test payload!";
    if (msg_send(sock, message, strlen(message)) == SOCKET_ERROR)
    {
        return err("Failed to send message");
    }
    
    if (msg_recv(sock, response_buf, &response_len) == SOCKET_ERROR)
    {
        return err("Failed to receive response");
    }

    closesocket(sock);
    WSACleanup();
    getchar();
    return 0;
}

int msg_send(SOCKET s, const char *msg, int len)
{
    int ret = send(s, msg, len, 0);
    if (ret > 0)
    {
        printf("[SEND][%d bytes] %.*s\n", len, len, msg);
    }
    return ret;
}

int msg_recv(SOCKET s, char *buf, int *len)
{
    int ret = recv(s, buf, RESP_MAX_LEN, 0);
    if (ret > 0)
    {
        *len = ret;
        buf[*len] = 0;
        printf("[RECV][%d bytes] %.*s\n", *len, *len, buf);
    }
    return ret;
}

int err(const char *msg)
{
    int err_code = WSAGetLastError();
    char *sys = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS, 
                   NULL, WSAGetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPTSTR)&sys, 0, NULL);
    fprintf(stderr, "[ERROR] %s (%d: %s)", msg, err_code, sys);
    LocalFree(sys);
    WSACleanup();
    getchar();
    return err_code;
}