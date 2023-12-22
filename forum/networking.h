#pragma once

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <utility>

constexpr int BUFFER_SIZE = 1024;

int create_socket(const char *ip, int port)
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        std::cerr << "Error creating socket\n";
        exit(EXIT_FAILURE);
    }

    sockaddr_in server_addr{};

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = ip ? inet_addr(ip) : INADDR_ANY;
    server_addr.sin_port = htons(port); // Port for HTTP

    if (bind(server_socket, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) == -1)
    {
        std::cerr << "Error binding socket\n";
        std::cerr << "IP address: " << ip << std::endl;
        std::cerr << "Port: " << port << std::endl;
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1)
    {
        std::cerr << "Error listening on socket\n";
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    return server_socket;
}

int connect_to_server(const char *ip, int port)
{
    // Create a socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        std::cerr << "Error creating socket\n";
        return -1;
    }

    // Specify the server address and port
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0)
    {
        std::cerr << "Error converting IP address\n";
        close(clientSocket);
        return -1;
    }

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        std::cerr << "Error connecting to the server\n";
        close(clientSocket);
        return -1;
    }

    return clientSocket;
}