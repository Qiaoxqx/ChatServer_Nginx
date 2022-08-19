#include "groupmodel.hpp"
#include "db.h"

// 1. 创建群组（群组的初始化操作）
bool GroupModel::createGroup(Group &group){
    // sql语句组装
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s','%s')", 
            group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 2. 加入群组（群组的增加操作）
bool GroupModel::addGroup(int userid, int groupid, string role){
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d,%d,'%s')", 
            groupid, userid, role.c_str());

    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            return true;
        }
    }
    return false;
}
// 3. 查询某用户所在群组消息
vector<Group> GroupModel::queryGroupMsg(int userid){
    char sql[1024] = {0};
    /*
        1. 根据userid在groupuser表中查询该用户所属群组的信息
        2. 根据获得的群组信息，查询属于该群组的所有用户的userid，与allgroup表一起进行多表联合查询，最终查询出用户的详细信息
    */
    sprintf(sql, "select a.id, a.groupname,a.groupdesc from allgroup a inner join\
            groupuser b on a.id = b.groupid where b.userid = %d", userid);
    // 存储所有群组group，群组group里存储的是包含的用户信息user
    vector<Group> groupVec;

    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row; // 逐行查询
            // 从groupuser表中查询出某userid所在的所有群组信息
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    // 查询群组的用户信息(user表和groupuser表联合查询)
    for(Group &group : groupVec){
        sprintf(sql, "select a.id, a.name,a.state,b.grouprole from user a inner join\
            groupuser b on a.id = b.userid where b.groupid = %d", group.getId());
        
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row; // 逐行查询
            // 从groupuser表中查询出某userid所在的所有群组信息
            while((row = mysql_fetch_row(res)) != nullptr){
                GroupUser user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        }
        return groupVec;
    }
}
// 4. 群组消息发送：除发送消息的userid之外，根据指定的groupid查询该群组的用户id列表，
//    将消息发送给群的其他成员
vector<int> GroupModel::queryGroupUsers(int userid, int groupid){
    // sql语句组装
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", 
            groupid, userid);
    // 存储查询到的userid
    vector<int> idVec;

    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                idVec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return idVec;
}