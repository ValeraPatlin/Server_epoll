#ifndef ESERVER_H
#define ESERVER_H

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <sstream>
#include <string>
#include <iomanip>
#include <unordered_map>
#include <ctime>

/*!
 * @brief Структура для хранения информации о клиенте.
 */
struct ClientData
{
    sockaddr_in address;    //!< Адрес клиента.
    socklen_t address_len;  //!< Длина адреса клиента.
    int fd;                 //!< File descriptor клиента.
    bool is_tcp;            //!< Флаг, указывающий, является ли соединение TCP.
};


/*!
 * @brief Класс, реализующий асинхронный сервер на основе epoll для обработки TCP и UDP соединений.
 */
class EServer
{
public:
    /*!
     * \brief EServer - Конструктор класса EServer.
     * \param events  - Максимальное количество событий для epoll.
     * \param tcpPort - Порт для прослушивания TCP соединений.
     * \param udpPort - Порт для прослушивания UDP соединений.
     */
    explicit EServer(size_t events, size_t tcpPort, size_t udpPort) noexcept;
    ~EServer() = default;

    explicit EServer(const EServer&) = delete;
    explicit EServer(EServer&& ) = delete;
    EServer& operator=(const EServer&) = delete;
    EServer& operator=(EServer&&) = delete;

    //! map для хранения данных о клиентах
    std::unordered_map<int, ClientData> client_map;

    /*!
     * \brief - setnonblocking - для установки сокета в неблокирующий режим
     * \param - sockfd File descriptor сокета
     */
    void setnonblocking(int sockfd) const;

    /*!
     * \brief handle_time_command - для обработки команды /time
     * \return Строка с текущим временем и датой в формате "YYYY-MM-DD HH:MM:SS"
     */
    std::string handle_time_command() const noexcept;

    /*!
     * \brief handle_stats_command - для обработки команды /stats
     * \return Строка со статистикой (общее количество подключившихся клиентов и подключенных в данный момент)
     */
    std::string handle_stats_command() const noexcept;

    /*!
     * \brief handle_shutdown_command - для обработки команды /shutdown
     * \param epoll_fd - File descriptor epoll
     */
    void handle_shutdown_command(int epoll_fd) const noexcept;

    /*!
     * \brief handle_tcp_connection - для обработки TCP соединения
     * \param epoll_fd - File descriptor epoll
     * \param event - Указатель на структуру epoll_event, содержащую информацию о событии
     */
    void handle_tcp_connection(int epoll_fd, epoll_event *event);

    /*!
     * \brief handle_tcp_data - для обработки данных от TCP клиента
     * \param epoll_fd - File descriptor epoll
     * \param event - Указатель на структуру epoll_event, содержащую информацию о событии
     */
    void handle_tcp_data(int epoll_fd, epoll_event *event);

    /*!
     * \brief handle_udp_data - для обработки UDP данных
     * \param epoll_fd - File descriptor epoll
     * \param udp_socket - File descriptor UDP сокета
     */
    void handle_udp_data(int epoll_fd, int udp_socket);

    /*!
     * \brief getMaxEvents - Возвращает максимальное количество событий для epoll
     * \return Максимальное количество событий
     */
    size_t getMaxEvents() const noexcept;

    /*!
     * \brief getTCPPort - Возвращает TCP port
     * \return TCP port
     */
    size_t getTCPPort() const noexcept;

    /*!
     * \brief getUdpPort - Возвращает UDP port
     * \return UDP port
     */
    size_t getUdpPort() const noexcept;

private:
    //! Максимальное количество событий для epoll
    const size_t MAX_EVENTS;

    //! Порт для прослушивания TCP соединений
    const size_t TCP_PORT;

    //! Порт для прослушивания UDP соединений
    const size_t UDP_PORT;

    //! переменные для статистики
    uint64_t total_connections;     //!< Общее число подключений
    uint64_t current_connections;   //!< Текущее число подключений

    //! Базовое значение для file descriptor-ов UDP клиентов
    static constexpr int UDP_CLIENT_FD_BASE = -1000;

    /*!
     * \brief handle_command - Обрабатывает команду и возвращает ответ
     * \param command - Строка с командой
     * \param epoll_fd - File descriptor epoll (необходим для /shutdown)
     * \return Строка с ответом на команду
     */
    std::string handle_command(const std::string& command, int epoll_fd) const;
};
#endif // ESERVER_H
