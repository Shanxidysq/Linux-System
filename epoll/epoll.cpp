#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace ox
{
    // connect连接类
    class Connect
    {
    public:
        // 读写缓冲区大小
        // reactor模型下需要有读写缓冲区
        static const int read_size = 1024;
        static const int write_size = 1024;

    public:
        // 读写缓冲区和fd文件socket描述符
        int fd;
        char read_buffer[read_size];
        char write_buffer[write_size];
        int read_len;
        int write_len;
        bool want_write;

        Connect(int fd) : fd(fd), read_len(0), write_len(0), want_write(false)
        {
            memset(read_buffer, 0, read_size);
            memset(write_buffer, 0, write_size);
        }

        ~Connect()
        {
            if (fd >= 0)
            {
                close(fd);
            }
        }

        Connect(const Connect &con) = delete;
        Connect &operator=(const Connect &con) = delete;
    };

    class EpollServer
    {
    private:
        // server_fd 监听套接字
        // epolln内核事件表
        int server_fd;
        int epoll_fd;
        bool running;
        std::unordered_map<int, std::unique_ptr<Connect>> connections;

        // 设置fd套接字非阻塞
        bool set_nonblocking(int fd)
        {
            // fcntl接口可以设置文件描述符属性 GETFL获取文件描述符的属性
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags == -1)
            {
                perror("fcntl F_GETFL");
                return false;
            }
            if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            {
                perror("fcntl F_SETFL");
                return false;
            }
            return true;
        }

        //  epoll操作接口 给fd注册epoll事件
        bool epoll_ctl_op(int op, int fd, uint32_t events, Connect *con)
        {
            struct epoll_event ev;
            ev.events = events;
            ev.data.ptr = con;

            if (epoll_ctl(epoll_fd, op, fd, &ev) == -1)
            {
                perror("epoll_ctl error");
                return false;
            }
            return true;
        }

        // 处理监听套接字的新连接
        void handle_accept()
        {
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);

            int client_fd = accept4(server_fd, (struct sockaddr *)&client_addr,
                                    &len, SOCK_NONBLOCK);
            if (client_fd == -1)
            {
                perror("accept4 error");
                return;
            }

            printf("New connection from: %s port: %d\n",
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port));

            auto con = std::make_unique<Connect>(client_fd);

            if (!epoll_ctl_op(EPOLL_CTL_ADD, client_fd,
                              EPOLLIN | EPOLLET | EPOLLRDHUP, con.get()))
            {
                close(client_fd);
                return;
            }

            connections[client_fd] = std::move(con);
        }

        // 处理读事件
        void handle_read(Connect *con)
        {
            while (true)
            {
                // 非阻塞套接字读取到EAGAIN退出
                ssize_t count = read(con->fd, con->read_buffer + con->read_len,
                                     Connect::read_size - con->read_len);

                if (count > 0)
                {
                    con->read_len += count;

                    // 查找是否收到完整的一行（以换行符结尾）
                    for (int i = 0; i < con->read_len; ++i)
                    {
                        if (con->read_buffer[i] == '\n')
                        {
                            // 处理收到的数据
                            int line_length = i + 1;
                            for (int j = 0; j < line_length - 1; ++j)
                            {
                                con->write_buffer[j] = std::toupper(con->read_buffer[j]);
                            }
                            con->write_buffer[line_length - 1] = '\n';
                            con->write_len = line_length;

                            // 移除已处理的数据
                            memmove(con->read_buffer, con->read_buffer + line_length,
                                    con->read_len - line_length);
                            con->read_len -= line_length;

                            // 尝试写入响应
                            handle_write(con);
                            break;
                        }
                    }
                }
                // ==0 对方关闭套接字
                else if (count == 0)
                {
                    // 对方关闭连接
                    std::cout << "Connection closed by client" << std::endl;
                    handle_close(con);
                    return;
                }
                else
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) // 修复这里
                    {
                        break;
                    }
                    else
                    {
                        perror("read error");
                        handle_close(con);
                        return;
                    }
                }
            }
        }

        // 处理写请求
        void handle_write(Connect *con)
        {
            if (con->write_len <= 0)
            {
                if (con->want_write)
                {
                    con->want_write = false;
                    epoll_ctl_op(EPOLL_CTL_MOD, con->fd,
                                 EPOLLIN | EPOLLET | EPOLLRDHUP, con);
                }
                return;
            }

            // 写缓冲区写数据，write_buffer write_len个字节
            // 返回就是成功写入的字节数
            ssize_t count = write(con->fd, con->write_buffer, con->write_len);

            // 成功写入，移动缓冲区数据
            if (count > 0)
            {
                con->write_len -= count;
                if (con->write_len > 0)
                {
                    memmove(con->write_buffer, con->write_buffer + count, con->write_len);
                }

                // 缓冲区内还有数据继续写入
                if (con->write_len > 0)
                {
                    if (!con->want_write)
                    {
                        con->want_write = true;
                        epoll_ctl_op(EPOLL_CTL_MOD, con->fd,
                                     EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP, con);
                    }
                }
                else
                {
                    if (con->want_write)
                    {
                        con->want_write = false;
                        epoll_ctl_op(EPOLL_CTL_MOD, con->fd,
                                     EPOLLIN | EPOLLET | EPOLLRDHUP, con);
                    }
                }
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) // 修复这里
                {
                    if (!con->want_write)
                    {
                        con->want_write = true;
                        epoll_ctl_op(EPOLL_CTL_MOD, con->fd,
                                     EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP, con);
                    }
                }
                else
                {
                    perror("write error");
                    handle_close(con);
                }
            }
        }

        // 关闭连接
        void handle_close(Connect *con)
        {
            connections.erase(con->fd);
        }

        // 处理错误
        void handle_error(Connect *conn)
        {
            std::cerr << "Error occurred on connection" << std::endl;
            handle_close(conn);
        }

    public:
        EpollServer() : server_fd(-1), epoll_fd(-1), running(false) {}

        ~EpollServer()
        {
            stop();
        }

        bool start(int port)
        {
            server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (server_fd == -1)
            {
                perror("socket error");
                return false;
            }

            int option = 1;
            if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
            {
                perror("setsockopt error");
                close(server_fd);
                return false;
            }

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            {
                perror("bind error");
                close(server_fd);
                return false;
            }

            if (listen(server_fd, SOMAXCONN) == -1)
            {
                perror("listen error");
                close(server_fd);
                return false;
            }

            epoll_fd = epoll_create1(0);
            if (epoll_fd == -1)
            {
                perror("epoll_create error");
                close(server_fd);
                return false;
            }

            // 创建服务器连接的Connect对象
            auto server_con = std::make_unique<Connect>(server_fd);

            // 注册监听事件到epoll的内核事件表
            if (!epoll_ctl_op(EPOLL_CTL_ADD, server_fd, EPOLLIN | EPOLLET, server_con.get()))
            {
                close(server_fd);
                close(epoll_fd);
                return false;
            }

            // 添加到map里面
            connections[server_fd] = std::move(server_con);

            running = true;
            std::cout << "Server started on port " << port << std::endl;
            return true;
        }

        void run()
        {
            const int MAX_EVENTS = 64;
            struct epoll_event events[MAX_EVENTS];

            while (running)
            {
                int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
                if (nfds == -1)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    perror("epoll_wait");
                    break;
                }

                for (int i = 0; i < nfds; ++i)
                {
                    Connect *conn = static_cast<Connect *>(events[i].data.ptr);

                    // 检查是否是服务器socket
                    if (conn->fd == server_fd)
                    {
                        handle_accept();
                        continue;
                    }

                    uint32_t event_mask = events[i].events;

                    if (event_mask & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
                    {
                        handle_error(conn);
                        continue;
                    }

                    if (event_mask & EPOLLIN)
                    {
                        handle_read(conn);
                    }

                    if (event_mask & EPOLLOUT)
                    {
                        handle_write(conn);
                    }
                }
            }
        }

        void stop()
        {
            running = false;
            if (epoll_fd >= 0)
            {
                close(epoll_fd);
                epoll_fd = -1;
            }
            if (server_fd >= 0)
            {
                close(server_fd);
                server_fd = -1;
            }
            connections.clear();
        }
    };
}

int main()
{
    ox::EpollServer server;
    if (!server.start(9898))
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Epoll server running on port 9898. Press Ctrl+C to stop." << std::endl;
    server.run();

    return 0;
}