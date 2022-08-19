#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>
#include <string>

using namespace std;

// 数据库类
class MySQL {
    public:
        // 初始化数据库连接
        MySQL();
        // 释放数据库连接资源
        ~MySQL();
        // 与数据库建立连接
        bool connect();
        // 更新数据
        bool update(string sql);
        // select操作 返回查询的结果
        MYSQL_RES* query(string sql);
        // 获取连接
        MYSQL* getConnection();
    private:
        MYSQL *_conn;
};

#endif