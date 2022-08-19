#include "db.h"
#include <muduo/base/Logging.h>

// 数据库配置信息 
static string server = "127.0.80.1"; 
static string user = "root"; 
static string password = "123456"; 
static string dbname = "chat";

// 初始化数据库连接
MySQL::MySQL(){
    _conn = mysql_init(nullptr);
}
// 释放数据库连接资源
MySQL::~MySQL(){
    if(_conn != nullptr){
        mysql_close(_conn);
    }
}
// 与数据库建立连接
bool MySQL::connect(){
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(), 
            password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if(p != nullptr){
        // C/C++代码默认的编码字符是ASCII，若不设置，从MySQL上拉下来的中文会显示？
        mysql_query(_conn, "set names gdk");
        LOG_INFO << "Connect MySQL success!";
    }else{
        LOG_INFO << "Connect MySQL fail!";
    }
    return p;
}
// 更新数据
bool MySQL::update(string sql){
    if(mysql_query(_conn, sql.c_str())){
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                << sql << "更新失败！";
        return false;
    }
    return true;
}
// select操作 返回查询的结果
MYSQL_RES* MySQL::query(string sql){
    if(mysql_query(_conn, sql.c_str())){
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                << sql << "查询失败！";
    }
    return mysql_use_result(_conn);
}

 MYSQL* MySQL::getConnection(){
    return _conn;
 }