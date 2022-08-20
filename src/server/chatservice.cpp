#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <string>
#include <vector>
using namespace muduo;
using namespace std;

// 获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作 绑定事件处理器
ChatService::ChatService(){
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REGIST_MSG, std::bind(&ChatService::regist, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务以及对应的Handler处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if(_redis.connect()){
        // 设置上报消息的回调函数
        // 2个参数是因为消息队列返回的是2个，int和string（id和message）
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }


}

// 服务器异常，业务重置
void ChatService::reset(){
    // 把online状态的用户设置为offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid){
    //记录错误日志，msg没有对应的事件处理回调
    // 若不存在，中括号会先添加一对儿
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end()){
        // LOG_ERROR << "msgid:" << msgid << "cannot find handler!";
        // string errstr = "msgid:" + msgid + "cannot find handler!";
        // lamda表达式(= 按值获取)：返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp){
            LOG_ERROR << "msgid:" << msgid << "cannot find handler!";
        };
    }else{
        // 若存在则返回存在的处理器
        return _msgHandlerMap[msgid];
    }
}

// 登陆业务 id pwd
// ORM框架：Object relation map 对象关系映射 业务层操作的都是对象 DAO(数据层)进行有数据的操作
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    json response;
    if(user.getPwd() == pwd && user.getId() == id){
        if(user.getState() == "online"){
            // 用户已经登录，不需要重复登录
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 1;
            response["errmsg"] = "该用户已经登录,请重新输入新账号";
            conn->send(response.dump()); 
        }else{
            // 记录用户连接信息 多线程情况下需要考虑线程安全问题 - 互斥锁
            {
                // 临界区代码段
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // 登录成功后，该用户以id为通道号向redis订阅channel监听消息
            _redis.subscribe(id);

            // 更新用户状态信息 state: offline -> online
            user.setState("online");
            _userModel.updateState(user);

            // 登录成功
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息，有的话主动发给用户
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty()){
                // 有离线消息
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            // 查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string> vec2;
                for(User &user : userVec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            conn->send(response.dump()); 
        }
    }else{
        // 登录失败 - 用户存在但密码错误
        if(user.getId() == id){
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "用户名存在但密码错误,请输入正确密码";
            conn->send(response.dump()); 
        }else{
            // 登录失败 - 用户不存在
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 3;
            response["errmsg"] = "该用户不存在,请进行注册后登录";
            conn->send(response.dump()); 
        }
    }
}

// 注册业务 <name, password>
void ChatService::regist(const TcpConnectionPtr &conn, json &js, Timestamp time){
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state =  _userModel.insert(user);
    json response;
    if(state){
        // 注册成功
        response["msgid"] = REGIST_MSG_ACK;
        // 响应消息一般都有错误标识
        response["errno"] = 0;
        response["id"] = user.getId();
        // 通过网络给客户端反馈,发送回去
        conn->send(response.dump()); 

    }else{
        // 注册失败
        response["msgid"] = REGIST_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump()); 
    }
}

// 处理注销业务
void ChatService::loginOut(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);  // 加锁实现线程安全
        auto it = _userConnMap.find(userid); // 迭代器遍历查找
        if(it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    // 用户注销的同时在redis中取消订阅通道
    _redis.unsubscribe(userid);

    // 更新用户状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn){ // 指向connection的智能指针
    User user;
    // 临界区代码段
    {
        lock_guard<mutex> lock(_connMutex);
            for(auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it){
            if(it->second == conn){
                // 从map表删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 用户注销的同时在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    // 更新用户的状态信息至表
    if(user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int toId = js["to"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toId);
        if(it != _userConnMap.end()){
            // toId在线，转发消息 服务器主动推送消息给toId用户
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线,若在线但不在connMap中，则说明该用户登录在其他服务器上
    User user = _userModel.query(toId);
    if(user.getState() == "online"){
        _redis.publish(toId, js.dump());
        return;
    }

    // 表示toId不在线,存储离线消息
    _offlineMsgModel.insert(toId, js.dump());

}

// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息 初始化
    json response;
    Group group(-1, name, desc);
    if(_groupModel.createGroup(group)){
        // 创建群组成功
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 0;
        response["id"] = group.getId();
        response["groupname"] = group.getName();
        // 存储群组创建人的信息
        _groupModel.addGroup(userid, group.getId(), "creator");

        conn->send(response.dump()); 
    }else{
        // 创建群组失败
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "群组创建失败!";

        conn->send(response.dump()); 
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    json response;
    GroupUser groupuser;

    if(_groupModel.addGroup(userid, groupid, "member")){
        // 加入群组成功
        response["msgid"] = ADD_GROUP_MSG_ACK;
        response["errno"] = 0;
        response["id"] = groupuser.getId();
        response["role"] = groupuser.getRole();
        conn->send(response.dump()); 
    }else{
        // 加入群组失败
        response["msgid"] = ADD_GROUP_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "加入群组失败!";

        conn->send(response.dump());    
    }
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> userVec = _groupModel.queryGroupUsers(userid, groupid);

    // 加锁保证操作安全&原子性
    lock_guard<mutex> lock(_connMutex);
    for(int id : userVec){
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end()){
            // 转发群消息
            it->second->send(js.dump());
        }else{
            User user = _userModel.query(id);
            if(user.getState() == "online"){
                _redis.publish(id, js.dump());
            }else{
                // 存储离线消息
                _offlineMsgModel.insert(id, js.dump());
            }

        }
    }    
}

// redis消息队列中获取订阅消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg){
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end()){
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}