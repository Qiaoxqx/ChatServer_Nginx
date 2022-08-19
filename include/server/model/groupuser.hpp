#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

// 群组中的用户可以作为User的子类，除了增添role（群组中的角色）外，其余可直接复用User的其他信息
class GroupUser:public User{
    public:
        void setRole(string role) {this->role = role;}
        string getRole() {return this->role;}
    private:
        string role;
};


#endif