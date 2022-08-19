#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"

#include <functional> // 函数对象绑定器
#include <string>
using namespace std;
using namespace placeholders; // 参数占位符
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop* loop,
               const InetAddress& listenAddr,
               const string& nameArg)
               :_server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册链接回调函数 _1
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    // 注册消息回调函数
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    _server.setThreadNum(10);

}

// 启动服务
void ChatServer::start(){
    _server.start();
}

// 上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn){
    // 客户端断开连接
    if(!conn->connected()){
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}
// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                        Buffer *buffer,
                        Timestamp time)
{
    /* 网络模块 */
    string buf = buffer->retrieveAllAsString();
    // 进行数据反序列化
    json js = json::parse(buf);
    /* 通过js["msgid"]绑定一个回调操作，一个id一个操作 获取业务handler => conn js time
       实现完全解耦网络模块和业务模块的代码，拆分开
       获取服务的唯一实例，传入消息id，获取消息id的事件处理器,然后回调事件处理器执行相应的业务处理
    */
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>()); // 用get强转成int型(想转成什么类型就用什么类型实例化get方法即可)
    // 回调消息绑好的事件处理器并执行那个相应的业务处理
    msgHandler(conn, js, time);

    /* 业务模块 */

}