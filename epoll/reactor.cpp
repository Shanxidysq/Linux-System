#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

namespace ox
{
    // 前向声明
    // reactor模型
    class ThreadPoolReactor;
    
    // 事件处理器接口
    class EventHandler
    {
    public:
        virtual ~EventHandler() = default;
        virtual void handle_event(uint32_t events) = 0;
        virtual int get_handle() const = 0;
    };

    // 任务接口
    class Task
    {
    public:
        virtual ~Task() = default;
        virtual void execute() = 0;
        virtual void complete() = 0;
    };

    // 线程池 线程池组件
    class ThreadPool
    {
    private:
        // 线程池
        std::vector<std::thread> workers;
        // 任务队列
        std::queue<std::shared_ptr<Task>> tasks;
        // 互斥同步
        std::mutex queue_mutex;
        std::condition_variable condition;
        // 线程池运行标志
        std::atomic<bool> stop;
        // reactor句柄
        ThreadPoolReactor* reactor;

    public:
        ThreadPool(size_t threads, ThreadPoolReactor* reactor);
        ~ThreadPool();

        // 模板函数
        template<class F>
        void enqueue(F&& task)
        {
            {
                // 在后面的多线程和多任务之间的同步中
                // 会使用到很多这种写法，例如块域的特性
                // 进行加锁 解锁等操作
                std::unique_lock<std::mutex> lock(queue_mutex);
                // 类型推导
                tasks.emplace(std::make_shared<ConcreteTask<std::decay_t<F>>>(std::forward<F>(task)));
            }
            // 通知任务队列
            condition.notify_one();
        }

        // 入队
        void enqueue_task(std::shared_ptr<Task> task)
        {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                tasks.push(task);
            }
            condition.notify_one();
        }

    private:
        void worker_loop();

        // 具体任务实现
        template<typename F>
        class ConcreteTask : public Task
        {
            // F 模板
        private:
            F func;
        public:
            // F&&f 参数类型 完美转发实现参数
            ConcreteTask(F&& f) : func(std::forward<F>(f)) {}
            void execute() override { func(); }
            void complete() override {}
        };
    };

    // 连接任务
    class ConnectionTask : public Task
    {
    private:
        // fd连接套接字 
        int fd;
        std::vector<char> data;
        ThreadPoolReactor* reactor;
        std::function<void(int, const std::vector<char>&)> callback;

    public:
        ConnectionTask(int fd, const char* buffer, size_t len, 
                      ThreadPoolReactor* reactor,
                      std::function<void(int, const std::vector<char>&)> cb)
            : fd(fd), data(buffer, buffer + len), reactor(reactor), callback(cb) {}

        void execute() override;
        void complete() override;
    };

    // 连接处理器
    class ConnectHandler : public EventHandler
    {
    public:
        static const int BUFFER_SIZE = 1024;

    private:
        int fd;
        std::vector<char> read_buffer;
        std::vector<char> write_buffer;
        size_t read_len;
        size_t write_len;
        ThreadPoolReactor* reactor;
        std::mutex write_mutex;
        
        // 回调函数
        using MessageCallback = std::function<void(int, const char*, size_t)>;
        using CloseCallback = std::function<void(int)>;
        
        MessageCallback message_callback;
        CloseCallback close_callback;

    public:
        ConnectHandler(int fd, ThreadPoolReactor* reactor) 
            : fd(fd), read_buffer(BUFFER_SIZE), write_buffer(BUFFER_SIZE),
              read_len(0), write_len(0), reactor(reactor) {}

        ~ConnectHandler()
        {
            if (fd >= 0)
            {
                close(fd);
            }
        }

        ConnectHandler(const ConnectHandler&) = delete;
        ConnectHandler& operator=(const ConnectHandler&) = delete;

        int get_handle() const override { return fd; }

        void set_message_callback(MessageCallback cb) { message_callback = cb; }
        void set_close_callback(CloseCallback cb) { close_callback = cb; }

        void send_data(const char* data, size_t len)
        {
            std::lock_guard<std::mutex> lock(write_mutex);
            if (write_len + len > write_buffer.size())
            {
                write_buffer.resize(write_len + len);
            }
            std::memcpy(write_buffer.data() + write_len, data, len);
            write_len += len;
        }

        void handle_event(uint32_t events) override
        {
            if (events & EPOLLERR || events & EPOLLHUP)
            {
                handle_error();
                return;
            }

            if (events & EPOLLIN)
            {
                handle_read();
            }

            if (events & EPOLLOUT)
            {
                handle_write();
            }
        }

    private:
        void handle_read()
        {
            while (true)
            {
                if (read_len >= read_buffer.size())
                {
                    read_buffer.resize(read_buffer.size() * 2);
                }

                ssize_t count = read(fd, read_buffer.data() + read_len, 
                                   read_buffer.size() - read_len);

                if (count > 0)
                {
                    read_len += count;
                    
                    // 将数据提交给线程池处理
                    if (message_callback && reactor)
                    {
                        auto task = std::make_shared<ConnectionTask>(
                            fd, read_buffer.data(), read_len, reactor, message_callback);
                        reactor->submit_task(task);
                    }
                    
                    read_len = 0; // 清空缓冲区，实际应用中可能需要更复杂的逻辑
                }
                else if (count == 0)
                {
                    // 连接关闭
                    if (close_callback)
                    {
                        close_callback(fd);
                    }
                    return;
                }
                else
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        break;
                    }
                    else
                    {
                        handle_error();
                        return;
                    }
                }
            }
        }

        void handle_write()
        {
            std::lock_guard<std::mutex> lock(write_mutex);
            if (write_len == 0) return;

            ssize_t count = write(fd, write_buffer.data(), write_len);

            if (count > 0)
            {
                write_len -= count;
                if (write_len > 0)
                {
                    std::memmove(write_buffer.data(), write_buffer.data() + count, write_len);
                }
            }
            else
            {
                if (!(errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    handle_error();
                }
            }
        }

        void handle_error()
        {
            if (close_callback)
            {
                close_callback(fd);
            }
        }
    };

    // 接受连接处理器
    class AcceptHandler : public EventHandler
    {
    private:
        int server_fd;
        ThreadPoolReactor* reactor;
        
        using NewConnectionCallback = std::function<void(int, ThreadPoolReactor*)>;
        NewConnectionCallback new_connection_callback;

    public:
        AcceptHandler(int port, ThreadPoolReactor* reactor);
        ~AcceptHandler();

        int get_handle() const override { return server_fd; }

        void set_new_connection_callback(NewConnectionCallback cb) 
        { 
            new_connection_callback = cb; 
        }

        void handle_event(uint32_t events) override
        {
            if (events & EPOLLIN)
            {
                handle_accept();
            }
        }

    private:
        void handle_accept();
    };

    // 基于线程池的Reactor
    class ThreadPoolReactor
    {
    private:
        int epoll_fd;
        std::atomic<bool> running;
        std::unordered_map<int, std::unique_ptr<EventHandler>> handlers;
        std::unique_ptr<ThreadPool> thread_pool;
        std::mutex handlers_mutex;

        // 完成队列相关
        int completion_fd;
        std::unique_ptr<EventHandler> completion_handler;
        std::mutex completion_mutex;
        std::queue<std::function<void()>> completion_queue;

        bool set_nonblocking(int fd)
        {
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags == -1) return false;
            return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
        }

        bool epoll_ctl_op(int op, int fd, uint32_t events, EventHandler* handler)
        {
            struct epoll_event ev;
            ev.events = events;
            ev.data.ptr = handler;

            return epoll_ctl(epoll_fd, op, fd, &ev) != -1;
        }

        // 完成事件处理器
        class CompletionHandler : public EventHandler
        {
        private:
            ThreadPoolReactor* reactor;
        public:
            CompletionHandler(int fd, ThreadPoolReactor* reactor) 
                : reactor(reactor) {}
                
            int get_handle() const override { return -1; } // 实际不使用
            void handle_event(uint32_t events) override
            {
                reactor->handle_completion();
            }
        };

    public:
        ThreadPoolReactor(size_t thread_count = std::thread::hardware_concurrency())
            : epoll_fd(-1), running(false)
        {
            // 创建完成事件fd
            completion_fd = eventfd(0, EFD_NONBLOCK);
            if (completion_fd == -1)
            {
                throw std::runtime_error("eventfd create failed");
            }
        }

        ~ThreadPoolReactor() 
        { 
            stop(); 
            if (completion_fd >= 0) close(completion_fd);
        }

        bool init()
        {
            epoll_fd = epoll_create1(0);
            if (epoll_fd == -1) return false;

            // 初始化线程池
            thread_pool = std::make_unique<ThreadPool>(std::thread::hardware_concurrency(), this);

            // 注册完成事件处理器
            completion_handler = std::make_unique<CompletionHandler>(completion_fd, this);
            if (!epoll_ctl_op(EPOLL_CTL_ADD, completion_fd, EPOLLIN, completion_handler.get()))
            {
                return false;
            }

            return true;
        }

        bool register_handler(std::unique_ptr<EventHandler> handler, uint32_t events)
        {
            int fd = handler->get_handle();
            if (!set_nonblocking(fd)) return false;
            
            std::lock_guard<std::mutex> lock(handlers_mutex);
            if (!epoll_ctl_op(EPOLL_CTL_ADD, fd, events | EPOLLET, handler.get())) 
                return false;
                
            handlers[fd] = std::move(handler);
            return true;
        }

        bool modify_handler(int fd, uint32_t events)
        {
            std::lock_guard<std::mutex> lock(handlers_mutex);
            auto it = handlers.find(fd);
            if (it == handlers.end()) return false;
            
            return epoll_ctl_op(EPOLL_CTL_MOD, fd, events | EPOLLET, it->second.get());
        }

        void remove_handler(int fd)
        {
            std::lock_guard<std::mutex> lock(handlers_mutex);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            handlers.erase(fd);
        }

        void submit_task(std::shared_ptr<Task> task)
        {
            thread_pool->enqueue_task(task);
        }

        // 在主线程中执行完成回调
        void post_completion(std::function<void()> callback)
        {
            {
                std::lock_guard<std::mutex> lock(completion_mutex);
                completion_queue.push(std::move(callback));
            }
            
            // 通知主线程有完成事件
            uint64_t value = 1;
            write(completion_fd, &value, sizeof(value));
        }

        void handle_completion()
        {
            // 读取eventfd，清空计数器
            uint64_t value;
            read(completion_fd, &value, sizeof(value));

            std::queue<std::function<void()>> local_queue;
            {
                std::lock_guard<std::mutex> lock(completion_mutex);
                local_queue.swap(completion_queue);
            }

            while (!local_queue.empty())
            {
                auto& callback = local_queue.front();
                if (callback) callback();
                local_queue.pop();
            }
        }

        void run()
        {
            const int MAX_EVENTS = 64;
            struct epoll_event events[MAX_EVENTS];

            running = true;
            while (running)
            {
                int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
                if (nfds == -1)
                {
                    if (errno == EINTR) continue;
                    break;
                }

                for (int i = 0; i < nfds; ++i)
                {
                    EventHandler* handler = static_cast<EventHandler*>(events[i].data.ptr);
                    handler->handle_event(events[i].events);
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
            handlers.clear();
        }

        ThreadPool* get_thread_pool() { return thread_pool.get(); }
    };

    // ThreadPool 实现
    ThreadPool::ThreadPool(size_t threads, ThreadPoolReactor* reactor) 
        : stop(false), reactor(reactor)
    {
        for (size_t i = 0; i < threads; ++i)
        {
            workers.emplace_back([this] { worker_loop(); });
        }
    }

    ThreadPool::~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }

    void ThreadPool::worker_loop()
    {
        while (!stop)
        {
            std::shared_ptr<Task> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition.wait(lock, [this] {
                    return stop || !tasks.empty();
                });
                if (stop && tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task->execute();
            task->complete();
        }
    }

    // ConnectionTask 实现
    void ConnectionTask::execute()
    {
        // 模拟业务处理 - 将数据转换为大写
        for (char& c : data)
        {
            c = std::toupper(c);
        }
        
        // 可以在这里添加更复杂的业务逻辑
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟处理时间
    }

    void ConnectionTask::complete()
    {
        // 通过Reactor在主线程中执行完成回调
        if (reactor && callback)
        {
            reactor->post_completion([this]() {
                callback(fd, data);
            });
        }
    }

    // AcceptHandler 实现
    AcceptHandler::AcceptHandler(int port, ThreadPoolReactor* reactor) 
        : reactor(reactor)
    {
        server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (server_fd == -1) throw std::runtime_error("socket error");

        int option = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
            throw std::runtime_error("bind error");

        if (listen(server_fd, SOMAXCONN) == -1)
            throw std::runtime_error("listen error");
    }

    AcceptHandler::~AcceptHandler()
    {
        if (server_fd >= 0) close(server_fd);
    }

    void AcceptHandler::handle_accept()
    {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        int client_fd = accept4(server_fd, (struct sockaddr*)&client_addr,
                                &len, SOCK_NONBLOCK);
        if (client_fd == -1) return;

        printf("New connection from: %s port: %d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        if (new_connection_callback) {
            new_connection_callback(client_fd, reactor);
        }
    }
}

// 应用层业务逻辑
class ThreadPoolEchoServer 
{
private:
    ox::ThreadPoolReactor reactor;
    std::unique_ptr<ox::AcceptHandler> accept_handler;

public:
    bool start(int port)
    {
        if (!reactor.init()) return false;

        try {
            accept_handler = std::make_unique<ox::AcceptHandler>(port, &reactor);
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }

        // 设置新连接回调
        accept_handler->set_new_connection_callback(
            [this](int fd, ox::ThreadPoolReactor* reactor) {
                this->on_new_connection(fd, reactor);
            });

        if (!reactor.register_handler(std::move(accept_handler), EPOLLIN)) {
            return false;
        }

        return true;
    }

    void run()
    {
        reactor.run();
    }

private:
    void on_new_connection(int fd, ox::ThreadPoolReactor* reactor)
    {
        auto conn_handler = std::make_unique<ox::ConnectHandler>(fd, reactor);
        
        // 设置消息回调 - 在工作线程中处理
        conn_handler->set_message_callback(
            [](int fd, const char* data, size_t len) {
                // 这个回调在工作线程中执行
                std::cout << "Processing message in worker thread: " 
                         << std::this_thread::get_id() << std::endl;
            });
            
        // 设置关闭回调
        conn_handler->set_close_callback(
            [this](int fd) {
                std::cout << "Connection closed: " << fd << std::endl;
                reactor.remove_handler(fd);
            });

        if (reactor->register_handler(std::move(conn_handler), EPOLLIN | EPOLLOUT)) {
            std::cout << "New connection handler registered: " << fd << std::endl;
        }
    }
};

int main()
{
    ThreadPoolEchoServer server;
    if (!server.start(9898))
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "ThreadPool Reactor server running on port 9898" << std::endl;
    std::cout << "Using " << std::thread::hardware_concurrency() << " worker threads" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    
    server.run();

    return 0;
}