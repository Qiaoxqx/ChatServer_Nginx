#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int){
    ChatService::instance()->reset();
    exit(0);
}

int main(){
    signal(SIGINT, resetHandler);

    EventLoop loop; // 事件循环
    InetAddress addr("127.0.80.1", 8000);
    ChatServer server(&loop, addr, "CharServer");

    server.start();
    loop.loop();

    return 0;

}