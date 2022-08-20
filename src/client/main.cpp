#include "json.hpp"
#include <ctime>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <functional>
#include <unordered_map>

#include <atomic>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "user.hpp"
#include "group.hpp"
#include "public.hpp"

using namespace std;
using json = nlohmann::json;

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 读写进程间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 读取消息线程
void readTaskHandler(int clientfd);
// 获取系统时间 - C++ 11系统库函数
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int);
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 聊天客户端：主线程发送消息，子线程接收消息
int main(int argc, char **argv){
    if(argc < 3){
        cerr << "Command invalid! Example: ./ChatClient 127.0.80.1 6000" << endl;
        exit(-1);
    }
    // 解析通过命令行传递的IP和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建客户端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(clientfd == -1){
        cerr << "socket create error!" << endl;
        exit(-1);
    }

    // 填写客户端需要连接的server信息IP+port
    sockaddr_in server;
    memset(&server, 0 ,sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // 客户端和服务端进行连接
    if(connect(clientfd, (sockaddr *)& server, sizeof(sockaddr_in)) == -1){
        cerr << "服务端连接失败!" << endl;
        close(clientfd);
        exit(-1);
    }

    // 初始化读写线程通用的信号量
    sem_init(&rwsem, 0, 0);

    // 连接服务器成功，启动读取消息的子线程
    std::thread readTask(readTaskHandler, clientfd);  // 底层调用pthread_create
    readTask.detach(); // 底层调用pthread_detach

    // 主线程用于接收用户输入信息，同时负责发送数据
    for(;;){
        // 显示首页面菜单，包含登录、注册、退出三个功能
        cout << "----------------** 欢迎来到集群聊天系统ChatServer **--------------------" << endl;
        cout << "----------------* ChatServer首页面 *--------------------" << endl;
        cout << "# 目前提供的服务有: \n" << endl;
        cout << "1. 登录" << endl;
        cout << "2. 注册" << endl;
        cout << "3. 退出" << endl;
        cout << "-----------------** THANK YOU & 感谢访问 **--------------------" << endl;
        cout << "# 请输入您选择的服务号(eg. 1->登录): " << endl;
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch(choice){
            // 登录
            case 1:
            {
                int id = 0;
                char pwd[50] = {0};
                cout << "# 请输入登录信息: " << endl;
                cout << "- 用户ID: ";
                cin >> id;
                cin.get();
                // cout << endl;
                cout << "- 密 码: ";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();

                g_isLoginSuccess = false;

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1){
                    cerr << "Send login msg error: " << request << endl;
                }
                
                // 等待信号量, 由子线程处理完登录的响应消息后进行通知
                sem_wait(&rwsem);

                if(g_isLoginSuccess){
                    // 登录成功后进入聊天主菜单页面
                    isMainMenuRunning = true;
                    mainMenu(clientfd);
                }
            }
            break;
            // 注册业务
            case 2:
            {
                char name[50] = {0};
                char pwd[50] = {0};
                cout << "# 请输入注册信息: " << endl;
                cout << "- 用户名: ";
                cin.getline(name, 50);
                cout << "- 密 码：";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = REGIST_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1){
                    cerr << "Send register msg error: " << request << endl;
                }
                
                // 等待信号量, 由子线程处理完登录的响应消息后进行通知
                sem_wait(&rwsem);
            }
            break;
            // 退出
            case 3:
                close(clientfd);
                sem_destroy(&rwsem);
                exit(0);
            default:
                cerr << "Invalid input!" << endl;
                break;
        }
    }
    return 0;
}

// 注册功能的响应处理
void doRegResonse(json &responsejs){
    // 注册失败
    if(responsejs["errno"].get<int>() != 0){
        cerr << "该用户已存在, 注册失败! " << endl;
    }else{
        cout << "用户注册成功! \n" << "用户ID: "<< responsejs["id"]
            << ", 请不要忘记! " << endl;
    }
}

// 登录功能的响应处理
void doLoginResponse(json &responsejs){
    // 登录失败
    if(responsejs["errno"].get<int>() != 0){
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }else{
        // 登录成功
        // 记录当前用户的id和name
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        // 记录当前用户的好友列表
        if(responsejs.contains("friends")){
            // 初始化
            g_currentUserFriendList.clear();

            vector<string> friendVec = responsejs["friends"];
            for(string &str : friendVec){
                json js = json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        if(responsejs.contains("groups")){
            // 初始化
            g_currentUserGroupList.clear();

            vector<string> groupVec1 = responsejs["groups"];
            for(string &groupstr : groupVec1){
                json groupjs = json::parse(groupstr);
                Group group;
                group.setId(groupjs["id"].get<int>());
                group.setName(groupjs["groupname"]);
                group.setDesc(groupjs["groupdesc"]);

                vector<string> groupVec2 = groupjs["users"];
                for(string &userstr : groupVec2){
                    json userjs = json::parse(userstr);
                    GroupUser groupuser;
                    groupuser.setId(userjs["id"].get<int>());
                    groupuser.setName(userjs["name"]);
                    groupuser.setState(userjs["state"]);
                    groupuser.setRole(userjs["role"]);
                    group.getUsers().push_back(groupuser);
                }
                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息，个人聊天信息或群组消息
        if(responsejs.contains("offlinemsg")){
            vector<string> vec = responsejs["offlinemsg"];
            for(string &str : vec){
                json js = json::parse(str);
                if(ONE_CHAT_MSG == js["msgid"].get<int>()){
                    cout << "@ 有个人聊天消息!" << endl;
                    cout << js["time"].get<string>() << "[" << js["id"] << js["name"].get<string>()
                        << ": " << js["msg"].get<string>() << endl;
                }else{
                    cout << "@ 有群消息!" << endl;
                    cout << "[" << js["groupid"] << "]: " << js["time"].get<string>()
                        << ": " << js ["msg"].get<string>() << endl; 
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

// 群组创建的响应处理
void doCreateGroupResonse(json &responsejs){
    // 创建失败
    if(responsejs["errno"].get<int>() != 0){
        cerr << "该群组已存在, 创建失败! " << endl;
    }else{
        cout << "群组创建成功! \n" << "群组ID: "<< responsejs["id"] << "\n" << "群组名: " << responsejs["groupname"]
            << ", 请不要忘记! " << endl;
    }
}

// 加入群组的响应处理
void doAddGroupResonse(json &responsejs){
    // 加入失败
    if(responsejs["errno"].get<int>() != 0){
        cerr << "该群组已加入, 加入失败! " << endl;
    }else{
        cout << "加入群组成功! \n" << "群组ID: "<< responsejs["id"] << "\n" << "群组名: " << responsejs["groupname"]
            << ", 请不要忘记! " << endl;
    }
}

// 子线程 -- 读取信息
void readTaskHandler(int clientfd){
    while(1){
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if(len == -1 || len == 0){
            close(clientfd);
            exit(-1);
        } 

        // 接收服务器转发的数据并反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if(ONE_CHAT_MSG == msgtype){
            cout << js["time"].get<string>() << "[" << js["id"]
                << js["name"].get<string>() << ": " << js["msg"].get<string>() << endl;
            continue;
        }

        if(GROUP_CHAT_MSG == msgtype){
            cout << "群消息[" << js["groupid"] << "]: " << js["groupname"].get<string>() << js["time"].get<string>()
                << "[" << js["id"] << "]" << js["name"].get<string>() << ": " << js["msg"].get<string>() << endl;
            continue;
        }

        // if(CREATE_GROUP_MSG_ACK == msgtype){
        //     doCreateGroupResonse(js);
        //     // 通知主线程, 群组创建成功
        //     sem_post(&rwsem);
        //     continue;
        // }

        // if(ADD_GROUP_MSG_ACK == msgtype){
        //     doAddGroupResonse(js);
        //     // 通知主线程, 群组创建成功
        //     sem_post(&rwsem);
        //     continue;
        // }

        if(REGIST_MSG_ACK == msgtype){
            doRegResonse(js);
            // 通知主线程，注册结果处理完成
            sem_post(&rwsem);
            continue;
        }

        if(LOGIN_MSG_ACK == msgtype){
            doLoginResponse(js);
            // 通知主线程，登录结果处理完成
            sem_post(&rwsem);
            continue;
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData(){
    cout << "----------------** 欢迎来到集群聊天系统ChatServer **--------------------" << endl;
    cout << "----------------* 个人聊天主页面 *--------------------" << endl;
    cout << "# 当前用户: " << endl;
    cout << "- 用户ID: " << g_currentUser.getId() << endl;
    cout << "- 用户名: " << g_currentUser.getName() << endl;
    cout << "##### 好友列表 #####" << endl;
    if(!g_currentUserFriendList.empty()){
        for(User &user : g_currentUserFriendList){
            cout << "|- ID -| - 用户名 - | - 状态 - |" << endl;
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "##### 群组列表 #####" << endl;
    if(!g_currentUserGroupList.empty()){
        for(Group &group : g_currentUserGroupList){
            cout << "|- ID -| - 组名 - | - 描述 - |" << endl;
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for(GroupUser &user : group.getUsers()){
                cout << "|- ID -| - 用户名 - | - 状态 - | - 角色 - |" << endl;
                cout << user.getId() << " " << user.getName() << " " << user.getState()  << " " << user.getRole() << endl;
            }
        }
    }
    cout << "-----------------------** THE END **--------------------------" << endl;
    cout << "-----------------** THANK YOU & 感谢访问 **--------------------" << endl;
}

// 用户操作帮助手册
void help(int fd = 0, string str = "");
// 聊天实现
void chat(int, string);
// 添加好友实现
void addFriend(int, string);
// 创建群组实现
void createGroup(int, string);
// 加入群组实现
void addGroup(int, string);
// 群组聊天实现
void groupChat(int, string);
// 登出
void loginOut(int, string);

// 聊天系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示系统支持的所有命令, 格式为-> help"},
    {"chat", "实现一对一聊天, 格式为-> chat:friendid:message"},
    {"addFriend", "实现好友添加, 格式为-> addFriend:friendid"},
    {"createGroup", "实现创建群组, 格式为-> createGroup:groupname:groupdesc"},
    {"addGroup", "实现加入群组, 格式为-> addGroup:groupid"},
    {"groupChat", "实现群组聊天, 格式为-> groupChat:groupid:message"},
    {"loginOut", "注销当前用户, 格式为-> loginOut"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addFriend", addFriend},
    {"createGroup", createGroup},
    {"addGroup", addGroup},
    {"groupChat", groupChat},
    {"loginOut", loginOut}};

// 聊天页面程序
void mainMenu(int clientfd){
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning){
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        // 存储命令
        string command;
        int idx = commandbuf.find(":");
        if(idx == -1){
            command = commandbuf;
        }else{
            // 分割输入命令字符串, 从而进行响应
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end()){
            cerr << "无效的输入命令!" << endl;
            continue;
        }

        // 调用相应命令的时间处理回调, mainMenu对修改功能关闭, 添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
    }
}

// 用户操作帮助手册
void help(int, string){
    cout << "< -- 命令列表 -- >" << endl;
    for(auto &p :commandMap){
        cout << p.first << ": " << p.second << endl;
    }
    cout << endl;
}

// 聊天实现
void chat(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "输入的聊天语句无效!" << endl;
        return; 
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "聊天消息发送错误! -> " << buffer << endl;
    }
}

// 添加好友实现
void addFriend(int clientfd, string str){
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friend"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "发送朋友信息错误! ->" << buffer << endl;
    }
}

// 创建群组实现
void createGroup(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "创建群组语句无效!" << endl;
        return; 
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "创建群组语句错误! -> " << buffer << endl;
    }
}

// 加入群组实现
void addGroup(int clientfd, string str){
    int groupid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "加入群组语句错误! -> " << buffer << endl;
    }
}

// 群组聊天实现
void groupChat(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "群组聊天语句无效!" << endl;
        return; 
    }
    string groupid = str.substr(0, idx);
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "发送群组消息错误! -> " << buffer << endl;
    }
}

// 登出
void loginOut(int clientfd, string str){
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "注销用户错误! -> " << buffer << endl;
    }else{
        isMainMenuRunning = false;
    }
}

string getCurrentTime(){
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);  
}