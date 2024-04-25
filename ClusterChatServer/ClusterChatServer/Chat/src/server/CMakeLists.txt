# 定义了一个 SRC_LIST变量，包含了指定目录下所有的源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./model MODEL_LIST)

# 指定源文件共同编译生成可执行文件  输出可执行文件的目录已在最外层 cmakelists中指定
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST})

# 指定可执行文件连接时需要依赖的库文件
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient pthread)