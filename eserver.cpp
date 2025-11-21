#include "eserver.h"


EServer::EServer(size_t events, size_t tcpPort, size_t udpPort) noexcept
    : MAX_EVENTS{events}, TCP_PORT{tcpPort}, UDP_PORT{udpPort},
    total_connections{0}, current_connections{0}
{}


void EServer::setnonblocking(int sockfd) const
{
    int flags = fcntl(sockfd, F_GETFL, 0);

    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        throw std::logic_error("fcntl F_GETFL");
    }

    flags |= O_NONBLOCK;

    if (fcntl(sockfd, F_SETFL, flags) == -1)
    {
        perror("fcntl F_SETFL O_NONBLOCK");
        throw std::logic_error("fcntl F_SETFL O_NONBLOCK");
    }
}

std::string EServer::handle_time_command() const noexcept
{
    time_t rawtime;
    tm *timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (timeinfo == nullptr)
    {
        std::cerr << "Error: localtime returned null" << std::endl;
        return "Error getting time";
    }

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

std::string EServer::handle_stats_command() const noexcept
{
    std::stringstream ss;
    ss << "Total connections: " << total_connections
       << ", Current connections: " << current_connections;
    return ss.str();
}

void EServer::handle_shutdown_command(int epoll_fd) const noexcept
{
    std::cout << "Shutting down the server..." << std::endl;
    for (auto const& [fd, client_data] : client_map)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        std::cout << "Connection closed: " << fd << std::endl;
    }

    close(epoll_fd);
    exit(0);
}

void EServer::handle_tcp_connection(int epoll_fd, epoll_event *event)
{
    sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    int client_fd = accept(event->data.fd, (sockaddr *) &client_address, &client_len);

    if (client_fd < 0)
    {
        throw std::logic_error("Error accepting connection");
    }

    setnonblocking(client_fd);

    ClientData new_client;
    new_client.address = client_address;
    new_client.address_len = client_len;
    new_client.fd = client_fd;
    new_client.is_tcp = true;
    client_map[client_fd] = new_client;

    epoll_event client_event;
    client_event.data.fd = client_fd;
    client_event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) < 0)
    {
        std::cerr << "epoll_ctl (add client)" << std::endl;
        close(client_fd);

        client_map.erase(client_fd);
        return;
    }

    ++total_connections;
    ++current_connections;

    std::cout << "New TCP connection from " << inet_ntoa(client_address.sin_addr)
              << ":" << ntohs(client_address.sin_port) << " (fd: " << client_fd << ")" << std::endl;
}

std::string EServer::handle_command(const std::string &command, int epoll_fd) const
{
    std::string response;
    std::cout << "\n command = " << command << std::endl;

    if (command == "/time")
    {
        response = handle_time_command();
    }
    else if (command == "/stats")
    {
        response = handle_stats_command();
    }
    else if (command == "/shutdown")
    {
        handle_shutdown_command(epoll_fd);
    }
    else
    {
        response = "Unknown command";
    }

    response += "\n";
    std::cout << "\n response = " << response << std::endl;

    return response;
}

void EServer::handle_tcp_data(int epoll_fd, epoll_event *event)
{
    char buffer[1024];
    ssize_t bytes_read;

    bytes_read = read(event->data.fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0)
    {
        buffer[bytes_read] = 0;

        std::cout << "Received from client (fd: " << event->data.fd << "): " << buffer << std::endl;

        if (buffer[0] == '/')
        {
            std::string command(buffer);
            std::string response = handle_command(command, epoll_fd);

            send(event->data.fd, response.c_str(), response.length(), 0);
        }
        else
        {
            send(event->data.fd, buffer, bytes_read, 0);
        }
    }
    else if (bytes_read == 0)
    {
        std::cout << "Client (fd: " << event->data.fd << ") disconnected" << std::endl;

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event->data.fd, NULL);
        close(event->data.fd);

        client_map.erase(event->data.fd);
        --current_connections;
    }
    else
    {
        std::cerr << "Error! read" << std::endl;

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event->data.fd, NULL);
        close(event->data.fd);

        client_map.erase(event->data.fd);
        --current_connections;
    }
}

void EServer::handle_udp_data(int epoll_fd, int udp_socket)
{
    char buffer[1024];
    sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    ssize_t bytes_received = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0,
                                      (sockaddr *) &client_address, &client_len);

    if (bytes_received > 0)
    {
        buffer[bytes_received] = 0;

        std::cout << "Received UDP data from " << inet_ntoa(client_address.sin_addr)
                  << ":" << ntohs(client_address.sin_port) << ": " << buffer << std::endl;

        if (buffer[0] == '/')
        {
            std::string command(buffer);
            std::string response = handle_command(command, epoll_fd);

            sendto(udp_socket, response.c_str(), response.length(), 0,
                   (sockaddr*)&client_address, client_len);
        }
        else
        {
            sendto(udp_socket, buffer, bytes_received, 0,
                   (sockaddr*)&client_address, client_len);
        }

        bool client_exists = false;
        int udp_client_fd = 0;

        for (auto const& [fd, client_data] : client_map)
        {
            if (!client_data.is_tcp &&
                client_data.address.sin_addr.s_addr == client_address.sin_addr.s_addr &&
                client_data.address.sin_port == client_address.sin_port)
            {
                client_exists = true;
                udp_client_fd = fd;
                break;
            }
        }

        if (!client_exists)
        {
            ++total_connections;
            ++current_connections;

            udp_client_fd = UDP_CLIENT_FD_BASE - total_connections;

            ClientData new_client;
            new_client.address = client_address;
            new_client.address_len = client_len;
            new_client.fd = udp_client_fd;
            new_client.is_tcp = false;
            client_map[udp_client_fd] = new_client;

            std::cout << "New UDP connection from " << inet_ntoa(client_address.sin_addr)
                      << ":" << ntohs(client_address.sin_port)
                      << " (fd: " << udp_client_fd << ")" << std::endl;
        }
    }
    else
    {
        std::cerr <<"recvfrom" << std::endl;
    }
}

size_t EServer::getMaxEvents() const noexcept
{
    return MAX_EVENTS;
}

size_t EServer::getTCPPort() const noexcept
{
    return  TCP_PORT;
}

size_t EServer::getUdpPort() const noexcept
{
    return  UDP_PORT;
}
