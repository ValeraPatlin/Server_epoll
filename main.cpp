#include "eserver.h"


int main()
{
    const size_t MAX_EVENTS = 64;
    const size_t TCP_PORT = 12345;
    const size_t UDP_PORT = 12346;

    EServer eserver{MAX_EVENTS, TCP_PORT, UDP_PORT};

    int tcp_socket{}, udp_socket{}, epoll_fd{};

    sockaddr_in server_address;
    epoll_event event, events[eserver.getMaxEvents()];

    try
    {
        if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            throw std::logic_error("socket (TCP)");
        }

        if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            throw std::logic_error("socket (UDP)");
        }

        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(eserver.getTCPPort());

        if (bind(tcp_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        {
            throw std::logic_error("bind (TCP)");
        }

        server_address.sin_port = htons(eserver.getUdpPort());

        if (bind(udp_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        {
            throw std::logic_error("bind (UDP)");
        }

        if (listen(tcp_socket, 5) < 0)
        {
            throw std::logic_error("listen");
        }

        if ((epoll_fd = epoll_create1(0)) < 0)
        {
            throw std::logic_error("epoll_create1");
        }

        eserver.setnonblocking(tcp_socket);
        eserver.setnonblocking(udp_socket);

        event.data.fd = tcp_socket;
        event.events = EPOLLIN | EPOLLET;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_socket, &event) < 0)
        {
            throw std::logic_error("eepoll_ctl (TCP)");
        }

        event.data.fd = udp_socket;
        event.events = EPOLLIN | EPOLLET;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_socket, &event) < 0)
        {
            throw std::logic_error("eepoll_ctl (UDP)");
        }

        std::cout << "Server listening on TCP port "
                  << eserver.getTCPPort() << " and UDP port " << eserver.getUdpPort() << std::endl;

        while (true)
        {
            int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

            if (num_events < 0)
            {
                if (errno == EINTR)
                {
                    std::cerr << "epoll_wait interrupted by signal, continuing..." << std::endl;
                    continue;
                }
                else
                {
                    throw std::logic_error("epoll_wait");
                }
            }

            for (int i = 0; i < num_events; ++i)
            {
                if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
                {
                    std::cerr << "epoll error" << std::endl;
                    close(events[i].data.fd);
                    continue;
                }

                if (events[i].data.fd == tcp_socket)
                {
                    eserver.handle_tcp_connection(epoll_fd, &events[i]);

                }
                else if (events[i].data.fd == udp_socket)
                {
                    eserver.handle_udp_data(epoll_fd, udp_socket);
                }
                else
                {
                    eserver.handle_tcp_data(epoll_fd, &events[i]);
                }
            }
        }
    }
    catch (std::logic_error& err)
    {
        std::cerr << "! ERROR! " << err.what() << std::endl;

        if (tcp_socket != -1)
            close(tcp_socket);

        if (udp_socket != -1)
            close(udp_socket);

        if (epoll_fd != -1)
            close(epoll_fd);
        return 1;
    }

    return 0;
}
