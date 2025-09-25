#ifndef _EPOLL_CLIENT_HPP_
#define _EPOLL_CLIENT_HPP_
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
// 连接类
class Connection
{
public:
    // 连接状态枚举
    enum State
    {
        CONNECTING,
        CONNECTED,
        CLOSED
    };
    Connection(int fd, const std::string &host, uint16_t port);
    ~Connection();
    // 获取fd 连接套接字
    int fd() const { return fd_; }
    // 获取连接状态
    State state() const { return state_; }
    // 设置连接状态
    void set_state(State s) { state_ = s; }
    // epoll io处理函数
    void handle_write();
    void handle_read();
    // 发送数据
    void send_data(const std::string &data);

private:
    int fd_;
    std::string host_;
    uint16_t port_;
    State state_;
    std::string read_buf_;
};

// ---------- Epoll ----------
class Epoll
{
public:
    Epoll();
    ~Epoll();
    void add(int fd, uint32_t events, void *ptr);
    void mod(int fd, uint32_t events, void *ptr);
    void del(int fd);
    std::vector<struct epoll_event> wait(int max_events = 16, int timeout_ms = 1000);

private:
    int epfd_;
};

// ---------- Client ----------
class Client
{
public:
    Client() = default;
    void connect(const std::string &host, uint16_t port);
    void loop();

private:
    void cleanup(int fd);
    Epoll epoll_;
    std::unordered_map<int, std::unique_ptr<Connection>> conns_;
};
#endif