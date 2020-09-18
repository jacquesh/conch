#ifndef __CONCH_CLIENT_H
#define __CONCH_CLIENT_H

#pragma comment (lib, "Ws2_32.lib")

typedef enum
{
    CONCH_SUCCESS,
    CONCH_UNKNOWN_ERROR,
    CONCH_NO_RESPONSE,
    // TODO: No values available for lease, not enough buffer space
} conch_result;

conch_result conch_lease_key(const char* set_name, char* output_buffer, unsigned int output_buffer_length);


#ifdef CONCH_CLIENT_IMPLEMENTATION
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>

#include <assert.h>
#include <stdio.h>

DWORD WINAPI keep_alive(void* param)
{
    SOCKET socket = (SOCKET)param;
    char buffer[256];
    int buffer_len = (int)sizeof(buffer);
    while(1)
    {
        int recv_result = recv(socket, buffer, buffer_len, 0);
        if (recv_result > 0)
        {
            int sendResult = send(socket, buffer, recv_result, 0);
            if(sendResult == SOCKET_ERROR)
            {
                printf("Keep-alive send failed with error: %d\n", WSAGetLastError());
                break;
            }
        }
        else if (recv_result == 0)
        {
            printf("Keep-alive connection closed by the remote peer\n");
            break;
        }
        else
        {
            printf("Keep-alive recv failed with error: %d\n", WSAGetLastError());
            break;
        }
    }

    printf("Stopping the keep alive!\n");
    closesocket(socket);
    return 0;
}

conch_result conch_lease_key(const char* set_name, char* output_buffer, unsigned int output_buffer_length)
{
    // TODO: Set (very short, ~50ms) timeouts on all of the above, possibly letting that be configured by the caller eventually
    SOCKET connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(connectSocket == INVALID_SOCKET)
    {
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (iResult != 0) {
            printf("WSAStartup failed with error: %d\n", iResult);
            return CONCH_UNKNOWN_ERROR; // TODO
        }

        // TODO: Check that we got WinSock 2.2?

        connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(connectSocket == INVALID_SOCKET)
        {
            printf("socket failed with error: %d\n", WSAGetLastError());
            WSACleanup();
            return CONCH_UNKNOWN_ERROR; // TODO
        }
    }

    // Connect to server.
    struct sockaddr_in addr_ipv4 = {0};
    addr_ipv4.sin_family = AF_INET;
    addr_ipv4.sin_port = htons(26624);
    addr_ipv4.sin_addr.S_un.S_addr = 0x0100007F; // localhost
    const struct sockaddr* addr = (const struct sockaddr*)&addr_ipv4;
    int connectError = connect(connectSocket, addr, sizeof(addr_ipv4));
    if(connectError != 0)
    {
        closesocket(connectSocket);
        printf("Unable to connect to server!\n");
        return CONCH_UNKNOWN_ERROR; // TODO
    }

    // Send the request
    size_t set_name_length = strlen(set_name);
    if(set_name_length > 256)
    {
        closesocket(connectSocket);
        printf("Given set name is too long!\n");
        return CONCH_UNKNOWN_ERROR; // TODO
    }
    unsigned char send_buffer[257];
    send_buffer[0] = (unsigned char)(set_name_length & 0xFF);
    memcpy(send_buffer+1, set_name, set_name_length);
    int sendResult = send(connectSocket, (char*)send_buffer, (int)set_name_length+1, 0);
    if(sendResult == SOCKET_ERROR)
    {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(connectSocket);
        return CONCH_UNKNOWN_ERROR; // TODO
    }
    printf("Bytes Sent: %d\n", sendResult);

    // Receive the response
    char recvbuf[256];
    int recvbuflen = (int)sizeof(recvbuf);
    int recv_result = recv(connectSocket, recvbuf, 1, 0);
    if (recv_result > 0)
    {
        printf("Bytes received: %d\n", recv_result);
        if(recvbuf[0] < 0)
        {
            return CONCH_UNKNOWN_ERROR; // TODO: Invalid length received
        }

        unsigned int recv_str_len = (unsigned int)recvbuf[0];
        if(recv_str_len == 0)
        {
            return CONCH_UNKNOWN_ERROR; // TODO: No values available
        }

        if(output_buffer_length < recv_str_len+1)
        {
            return CONCH_UNKNOWN_ERROR; // TODO: Output buffer too small
        }

        unsigned int total_received = recv_result;
        while(total_received < recv_str_len+1)
        {
            recv_result = recv(connectSocket, recvbuf+total_received, recvbuflen-total_received, 0);
            if(recv_result > 0)
            {
                total_received += (unsigned int)recv_result;
            }
            else if(recv_result == 0)
            {
                closesocket(connectSocket);
                printf("Connection closed while receiving key data\n");
                return CONCH_NO_RESPONSE; // TODO
            }
            else
            {
                closesocket(connectSocket);
                printf("recv failed while receiving key data with error: %d\n", WSAGetLastError());
                return CONCH_UNKNOWN_ERROR; // TODO
            }
        }

        memcpy(output_buffer, &recvbuf[1], recv_str_len);
        output_buffer[recv_str_len] = '\0';
    }
    else if(recv_result == 0)
    {
        closesocket(connectSocket);
        printf("Connection closed\n");
        return CONCH_NO_RESPONSE; // TODO
    }
    else
    {
        closesocket(connectSocket);
        printf("recv failed with error: %d\n", WSAGetLastError());
        return CONCH_UNKNOWN_ERROR; // TODO
    }

    // Start the keepalive thread
    assert(sizeof(connectSocket) <= sizeof(void*));
    HANDLE thread = CreateThread(NULL, 0, keep_alive, (void*)connectSocket, 0, NULL);
    if(thread == NULL)
    {
        closesocket(connectSocket);
        printf("CreateThread failed with error: %lu\n", GetLastError());
        return CONCH_UNKNOWN_ERROR; // TODO
    }

    return CONCH_SUCCESS;
}
#endif // CONCH_CLIENT_IMPLEMENTATION

#endif // __CONCH_CLIENT_H
