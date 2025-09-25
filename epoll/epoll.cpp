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

// 封装connect类，管理连接套接字，非阻塞io模式

namespace ox
{
    // connect 连接管理类
    class Connect
    {
    public:
        // 读写缓冲区大小
        static const int read_size = 1024;
        static const int write_size = 1024;

    public:
        // 连接套接字fd
        int fd;

        // 读写缓冲区和读写字节数
        // 缓冲区
        char read_buffer[read_size];
        char write_buffer[write_size];
        // len
        int read_len;
        int write_len;

        // 标记是否希望关注写事件
        bool want_write;

        Connect(int fd) : fd(fd), read_len(0), write_len(0), want_write(false)
        {
            memset(read_buffer, 0, read_size);
            memset(write_buffer, 0, write_size);
        }

        ~Connect()
        {
            // 存在连接套接字
            if (fd >= 0)
            {
                close(fd);
            }
        }

        // 删除拷贝构造和拷贝辅助
        Connect(const Connect &con) = delete;
        Connect &operator=(const Connect &con) = delete;
    };

    // Epoll 服务器类
    class EpollServer
    {
    private:
        // server fd 监听套接字
        int server_fd;
        // epoll fd  epoll fd 管理epoll的内核事件表
        int epoll_fd;
        // 运行状态
        bool running;

        std::unordered_map<int, std::unique_ptr<Connect>> connectiongs;

        /**
         * @brief 设置fd为非阻塞io模式
         *
         * @param fd 文件描述符
         *
         * @return true 设置成功 false 设置失败
         */
        bool set_nonblocking(int fd)
        {
            // fcntl接口控制文件描述符属性的
            // F_GETFL 获取文件状态
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags == -1)
            {
                perror("fcntl F_GETFL");
                return false;
            }
            // 设置套接字非阻塞模式
            if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            {
                perror("fcntl F_SETFL");
                return false;
            }
            return true;
        }

        /**
         * @brief fd 文件描述符添加到epoll的内核事件表中
         *
         * @param op 操作
         * @param fd 需要注册到内核事件表的文件描述符
         * @param events 事件
         * @param con evnet.data.pt指向con 指向数据对象
         *
         * @return true 成功 false 失败
         */
        bool epoll_ctl_op(int op, int fd, uint32_t events, Connect *con)
        {
            struct epoll_event ev;
            ev.events = events;
            // epoll_event 事件存在一个data_ptr指针指向数据对象
            ev.data.ptr = con;

            // 添加文件描述符到epoll的内核事件表当中
            int ret = epoll_ctl(epoll_fd, op, fd, &ev);
            if (-1 == ret)
            {
                perror("epoll_ctl_op epoll_ctl error\n");
                return false;
            }
            return true;
        }

        /**
         * @brief 处理新连接
         */
        void handle_accept()
        {
            // client_addr socket地址
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);

            // 创建非阻塞连接套接字
            int client_fd = accept4(server_fd, (struct sockaddr *)&client_addr,
                                    &len, SOCK_NONBLOCK);
            if (client_fd == -1)
            {
                perror("handle_accept accept4 error]n");
                return;
            }

            printf("new connect form :%s port: %d \n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            std::unique_ptr<Connect> con(new Connect(client_fd));

            // 添加到epoll的内核事件表
            int ret = epoll_ctl_op(EPOLL_CTL_ADD, client_fd,
                                   EPOLLIN | EPOLLET | EPOLLRDHUP, con.get());
            if (!ret)
            {
                close(client_fd);
                perror("handle_accept epoll_ctl_op error\n");
                return;
            }
            // 移动con到map中
            connectiongs[client_fd] = std::move(con);
        }

        /**
         * @brief 处理读事件
         */
        void handle_read(Connect *con)
        {
            while (true)
            {
                // ET模式下必须一次处理完毕数据 即读取到EAGAIN
                size_t count = read(con->fd,
                                    con->read_buffer + con->read_len, Connect::read_size - con->read_len);
                // 正常读取到数据
                if (count > 0)
                {
                    con->read_len += count;
                    // read_len 大于0 读取到有效数据 并且是一个完整的字符串
                    if (con->read_len > 0 && con->read_buffer[con->read_len - 1] == '\n')
                    {
                        // 将收到的消息转为大写并准备回复
                        for (size_t i = 0; i < con->read_len - 1; ++i)
                        {
                            // 读取到的字符串转大写 写入 write_buffer
                            con->write_buffer[i] = std::toupper(con->read_buffer[i]);
                        }
                        con->write_buffer[con->read_len - 1] = '\n';
                        con->write_len = con->read_len;

                        // 重置读缓冲区
                        con->read_len = 0;

                        // 尝试立即写入响应
                        handle_write(con);
                    }
                }
                else if (count == 0)
                {
                    // 对方关闭socket连接
                    std::cout << "Connection closed by client" << std::endl;
                    handle_close(con);
                    return;
                }
                else
                {
                    // 错误处理
                    if (errno == EAGAIN | errno == EWOULDBLOCK)
                    {
                        // 非阻塞io读取到EAGAIN 正常情况退出
                        break;
                    }
                    else
                    {
                        // 错误处理
                        perror("read");
                        handle_close(con);
                        return;
                    }
                }
            }
        }

        /**
         * @brief 处理写
         */
        void handle_write(Connect *con)
        {
            if (con->write_len == 0)
            {
                // 没有数据要发送，移除写监听（如果之前设置了）
                if (con->want_write)
                {
                    con->want_write = false;
                    epoll_ctl_op(EPOLL_CTL_MOD, con->fd, EPOLLIN | EPOLLET, con);
                }
                return;
            }
            // 发送写缓冲区里面的数据
            size_t count = write(con->fd, con->write_buffer, con->write_len);
            // count 成功返回就是写入的字节数
            if (count >= 0)
            {
                // 如果还有数据就是移动数据
                con->write_len -= count;
                if (con->write_len > 0)
                {
                    // 移动数据 dest 目标 src 移动位置 len 移动多少数据
                    memmove(con->write_buffer, con->write_buffer + count, con->write_len);
                }

                if (con->write_len > 0)
                {
                    // 还有数据需要写入 继续监听写事件
                    //  还有数据要写，需要监听写事件
                    if (!con->want_write)
                    {
                        con->want_write = true;
                        epoll_ctl_op(EPOLL_CTL_MOD, con->fd, EPOLLIN | EPOLLOUT | EPOLLET, con);
                    }
                }
                else
                {
                    // 所有数据已写完，移除写监听
                    if (con->want_write)
                    {
                        con->want_write = false;
                        epoll_ctl_op(EPOLL_CTL_MOD, con->fd, EPOLLIN | EPOLLET, con);
                    }
                }
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 内核缓冲区已满，需要监听写事件
                    if (!con->want_write)
                    {
                        con->want_write = true;
                        epoll_ctl_op(EPOLL_CTL_MOD, con->fd, EPOLLIN | EPOLLOUT | EPOLLET, con);
                    }
                }
                else
                {
                    // 真正的错误
                    perror("write");
                    handle_close(con);
                }
            }
        }

        /**
         * @brief 关闭close连接
         */
        void handle_close(Connect *con)
        {
            // unique_ptr 自动释放对象
            connectiongs.erase(con->fd);
        }

        // 处理错误事件
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

        /**
         * @brief 启动epoll服务器的监听
         * @param port 设置连接套接字监听端口
         * @return true 启动成功 false 启动失败
         */
        bool start(int port)
        {
            // 监听套接字
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

            if (!epoll_ctl_op(EPOLL_CTL_ADD, server_fd, EPOLLIN | EPOLLET, server_con.get()))
            {
                close(server_fd);
                close(epoll_fd);
                return false;
            }

            connectiongs[server_fd] = std::move(server_con);

            running = true;
            std::cout << "Server started on port " << port << std::endl;
            return true;
        }

        // 运行事件循环
        void run()
        {
            const int MAX_EVENTS = 64;
            struct epoll_event events[MAX_EVENTS];

            while (running)
            {
                // 返回多少个准备好的套接字
                int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
                if (nfds == -1)
                {
                    if (errno == EINTR)
                    {
                        continue; // 被信号中断，继续等待
                    }
                    perror("epoll_wait");
                    break;
                }

                for (int i = 0; i < nfds; ++i)
                {
                    // 处理服务器socket（新连接）
                    if (events[i].data.fd == server_fd)
                    {
                        handle_accept();
                        continue;
                    }

                    // 处理客户端连接
                    Connect *conn = static_cast<Connect *>(events[i].data.ptr);
                    uint32_t event_mask = events[i].events;

                    // 处理错误和挂起事件
                    if (event_mask & (EPOLLERR | EPOLLHUP))
                    {
                        handle_error(conn);
                        continue;
                    }

                    // 处理可读事件
                    if (event_mask & EPOLLIN)
                    {
                        handle_read(conn);
                    }

                    // 处理可写事件
                    if (event_mask & EPOLLOUT)
                    {
                        handle_write(conn);
                    }
                }
            }
        }

        // 停止服务器
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
            connectiongs.clear();
        }
    };
}

int main()
{
    ox::EpollServer server;
    if (!server.start(5001))
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Epoll server running. Press Ctrl+C to stop." << std::endl;

    // 运行事件循环
    server.run();

    return 0;
}
