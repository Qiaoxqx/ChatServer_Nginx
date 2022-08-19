#include "friendmodel.hpp"
#include "db.h"

// 添加好友关系 （正常情况下好友列表保存在客户端）
void FriendModel::insert(int userid, int friendid){
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d, %d)", userid, friendid);
    
    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

// 返回用户好友列表 friendid -> username/state(从user表中)
vector<User> FriendModel::query(int userid){
    char sql[1024] = {0};

    sprintf(sql, "select a.id,a.name,a.state from user a inner join friend b on b.friendid = a.id where b.userid = %d", userid);
    
    MySQL mysql;
    vector<User> vec;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}
