#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include "user.hpp"
#include <vector>
using namespace std;

// 提供好友信息，维护好友信息的操作接口方法
class FriendModel{
    public:
        // 添加好友关系 （正常情况下好友列表保存在客户端）
        void insert(int userid, int friendid);

        // 返回用户好友列表 friendid -> username/state(从user表中)
        // 两个表的联合查询
        vector<User> query(int userid);
};

#endif