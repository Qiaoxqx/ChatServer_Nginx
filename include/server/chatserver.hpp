#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string.h>
using namespace muduo;
using namespace muduo::net; // 引入muduo::net的命名空间可以简写函数/变量名

/* 
  聊天服务器的主类 基于muduo库
  好处：借助封装好的muduo网络库能实现I/O代码和业务代码区分开
  步骤：
    1. 组合TcpServer对象
    2. 创建Eventloop事件循环对象的指针
    3. 明确TcpServer构造函数需要的参数类型，输出Chatserver构造函数
    4. 在构造函数中注册处理连接的回调函数和处理读写事件的回调函数
    5. 根据实际设置合适的线程数量
*/
class ChatServer{
    public:
        // 初始化聊天服务器对象
        ChatServer(EventLoop* loop,
               const InetAddress& listenAddr,
               const string& nameArg);
        // 启动服务
        void start();
    private:
        // 上报链接相关信息的回调函数，当有新连接或连接中断时调用
        void onConnection(const TcpConnectionPtr&);
        // 上报读写事件相关信息的回调函数，当有新数据到来时调用
        void onMessage(const TcpConnectionPtr&,
                                Buffer*,
                                Timestamp);
        TcpServer _server;  // 组合muduo库，实现服务器功能的类对象
        EventLoop *_loop;   // 指向事件循环对象的指针
};

#endif