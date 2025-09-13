#include "epoll_client.hpp"
#include <iostream>
#include <stdexcept>

static void throw_error(const std::string &msg)
{
    throw std::runtime_error(msg + ": " + std::strerror(errno));
}

void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ---------- Connection ----------
Connection::Connection(int fd, const std::string &host, uint16_t port)
    : fd_(fd), host_(host), port_(port), state_(CONNECTING)
{
    set_nonblock(fd_);
}

Connection::~Connection() { ::close(fd_); }

void Connection::handle_write()
{
    if (state_ == CONNECTING)
    {
        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0)
        {
            std::cerr << "connect failed: " << strerror(err) << std::endl;
            state_ = CLOSED;
            return;
        }
        state_ = CONNECTED;
        std::cout << "connected to " << host_ << ":" << port_ << std::endl;
    }
}

void Connection::handle_read()
{
    char buf[4096];
    ssize_t n = recv(fd_, buf, sizeof(buf), 0);
    if (n > 0)
    {
        read_buf_.append(buf, n);
        std::cout << "recv: " << std::string(buf, n) << std::endl;
    }
    else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
    {
        state_ = CLOSED;
    }
}

void Connection::send_data(const std::string &data)
{
    ssize_t n = send(fd_, data.data(), data.size(), MSG_NOSIGNAL);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        state_ = CLOSED;
    }
}

// ---------- Epoll ----------
Epoll::Epoll() : epfd_(epoll_create1(EPOLL_CLOEXEC))
{
    if (epfd_ < 0)
        throw_error("epoll_create1");
}
Epoll::~Epoll() { ::close(epfd_); }

void Epoll::add(int fd, uint32_t events, void *ptr)
{
    struct epoll_event ev{};
    ev.events = events;
    ev.data.ptr = ptr;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) < 0)
        throw_error("epoll_ctl ADD");
}

void Epoll::mod(int fd, uint32_t events, void *ptr)
{
    struct epoll_event ev{};
    ev.events = events;
    ev.data.ptr = ptr;
    if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) < 0)
        throw_error("epoll_ctl MOD");
}

void Epoll::del(int fd)
{
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) < 0)
        throw_error("epoll_ctl DEL");
}

std::vector<struct epoll_event> Epoll::wait(int max_events, int timeout_ms)
{
    std::vector<struct epoll_event> evs(max_events);
    int nfds = epoll_wait(epfd_, evs.data(), max_events, timeout_ms);
    if (nfds < 0)
        throw_error("epoll_wait");
    evs.resize(nfds);
    return evs;
}

// ---------- Client ----------
void Client::connect(const std::string &host, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw_error("socket");

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
        throw_error("inet_pton");

    set_nonblock(fd);
    int rc = ::connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS)
        throw_error("connect");

    auto conn = std::make_unique<Connection>(fd, host, port);
    epoll_.add(fd, EPOLLOUT | EPOLLIN | EPOLLRDHUP | EPOLLET, conn.get());
    conns_[fd] = std::move(conn);
}

void Client::cleanup(int fd)
{
    epoll_.del(fd);
    conns_.erase(fd);
    std::cout << "connection closed\n";
}

void Client::loop()
{
    while (true)
    {
        auto events = epoll_.wait();
        for (auto &ev : events)
        {
            auto *conn = static_cast<Connection *>(ev.data.ptr);
            int fd = conn->fd();
            if (ev.events & (EPOLLERR | EPOLLHUP))
            {
                cleanup(fd);
                continue;
            }
            if (ev.events & EPOLLOUT)
            {
                conn->handle_write();
                if (conn->state() == Connection::CONNECTED)
                {
                    epoll_.mod(fd, EPOLLIN | EPOLLRDHUP | EPOLLET, conn);
                    conn->send_data("hello from epoll client\n");
                }
            }
            if (ev.events & EPOLLIN)
            {
                conn->handle_read();
            }
            if (ev.events & EPOLLRDHUP || conn->state() == Connection::CLOSED)
            {
                cleanup(fd);
            }
        }
    }
}