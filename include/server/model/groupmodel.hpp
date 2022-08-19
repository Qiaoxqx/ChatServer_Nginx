#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <string>
#include <vector>
using namespace std;

// 用于维护群组信息的操作接口方法
class GroupModel{
    public:
        // 1. 创建群组（群组的初始化操作）
        bool createGroup(Group &group);
        // 2. 加入群组（群组的增加操作）
        bool addGroup(int userid, int groupid, string role);
        // 3. 查询某用户所在群组消息
        vector<Group> queryGroupMsg(int userid);
        // 4. 群组消息发送：除发送消息的userid之外，根据指定的groupid查询该群组的用户id列表，
        //    将消息发送给群的其他成员
        vector<int> queryGroupUsers(int userid, int groupid);
};

#endif