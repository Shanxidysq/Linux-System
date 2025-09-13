#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

// ---------- 工具 ----------
void set_nonblock(int fd);

// ---------- Connection ----------
class Connection {
public:
    enum State { CONNECTING, CONNECTED, CLOSED };
    Connection(int fd, const std::string& host, uint16_t port);
    ~Connection();

    int         fd() const      { return fd_; }
    State       state() const   { return state_; }
    void        set_state(State s) { state_ = s; }
    void        handle_write();
    void        handle_read();
    void        send_data(const std::string& data);

private:
    int         fd_;
    std::string host_;
    uint16_t    port_;
    State       state_;
    std::string read_buf_;
};

// ---------- Epoll ----------
class Epoll {
public:
    Epoll();
    ~Epoll();
    void add(int fd, uint32_t events, void* ptr);
    void mod(int fd, uint32_t events, void* ptr);
    void del(int fd);
    std::vector<struct epoll_event> wait(int max_events = 16, int timeout_ms = 1000);

private:
    int epfd_;
};

// ---------- Client ----------
class Client {
public:
    Client() = default;
    void connect(const std::string& host, uint16_t port);
    void loop();

private:
    void cleanup(int fd);
    Epoll epoll_;
    std::unordered_map<int, std::unique_ptr<Connection>> conns_;
};