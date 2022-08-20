#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <functional>
#include <muduo/net/TcpConnection.h>
#include <mutex> // 互斥锁

using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders; // 参数占位符

#include "usermodel.hpp"
#include "offlinemsgmodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "json.hpp"
#include "redis.hpp"
using json = nlohmann::json;

// 定义一个事件处理器 消息id->事件回调
// 表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

// ChatServer业务类
class ChatService{
    public:
        // 获取单例对象的接口函数
         static ChatService* instance();
        // 登录业务
        void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
        // 注册业务
        void regist(const TcpConnectionPtr &conn, json &js, Timestamp time);
        // 一对一聊天服务
        void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
        // 添加好友业务
        void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
        // 创建群组业务
        void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
        // 加入群组业务
        void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
        // 群组聊天业务
        void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

        // 处理注销业务
        void loginOut(const TcpConnectionPtr &conn, json &js, Timestamp time);
        // 服务器异常的业务重置方法
        void reset();
        // 获取消息对应的处理器
        MsgHandler getHandler(int msgid);
        // 处理客户端异常退出
        void clientCloseException(const TcpConnectionPtr &conn);

        // redis消息队列中获取订阅消息
        void handleRedisSubscribeMessage(int, string);

    private:
        // 构造函数私有化
        ChatService();

        // 消息处理器表->消息id对应的处理操作
        // 存储消息id以及其对应的业务处理方法
        unordered_map<int, MsgHandler> _msgHandlerMap;

        // 存储在线用户的通信连接(一个用户对应一个conn) 访问一定要注意线程安全
        unordered_map<int, TcpConnectionPtr> _userConnMap;
        
        // 定义互斥锁，保证_userConnMap的线程安全
        mutex _connMutex;
        
        // 数据操作类对象
        UserModel _userModel;
        OfflineMsgModel _offlineMsgModel;
        FriendModel _friendModel;
        GroupModel _groupModel;

        // redis消息队列操作对象
        Redis _redis;
};


#endif