# 定义源文件列表实现同时编译多个源文件
# 定义了一个SRC_LIST变量，包含了该目录下所有的源文件, SRC_LIST代表. 
aux_source_directory(. SRC_LIST)
aux_source_directory(./database DB_LIST)
aux_source_directory(./model MODEL_LIST)


# 指定生成可执行文件 ChatServer会被保存在bin文件夹下
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST})
# 指定可执行文件链接时需要依赖的库文件
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient pthread)