# 附属源文件目录 定义了一个 SRC_LIST变量，包含了该目录下所有的源文件
aux_source_directory(. SRC_LIST)

# 指定生成的可执行文件
add_executable(ChatClient ${SRC_LIST})

# 指定可执行文件链接时需要依赖的库
target_link_libraries(ChatClient pthread)   # 客户端包含两个线程，读线程和写线程