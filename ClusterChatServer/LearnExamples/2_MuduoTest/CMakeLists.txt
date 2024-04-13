# 程序要求的 CMake版本最低版本
cmake_minimum_required(VERSION 3.0)
# 给项目取名
project(MuduoServerBaseVersion)

# 配置编译选项，比如 -g
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FALGS} -g)

# 指定搜索的子目录
# add_subdirectory(someSubDir)

# 配置头文件搜索路径
# include_directories()

# 配置库文件搜索路径
# link_directories()

# 设置需要编译的源文件列表      ./表示当前路径，不加也可以，当然也可以指定其他路径的源文件
set(SRC_LIST ./src/main.cpp)

# 设置可执行文件最终存储的路径   ${PROJECT_SOURCE_DIR} 有个是 CMakeList.txt所在路径？
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 把 . 指定路径下的所有源文件名字放入变量名 SRC_LIST 里面
# aux_source_directory(. SRC_LIST)

# 通过源文件生成可执行文件          除此外还可以生成库文件等...(add_library())
add_executable(main ${SRC_LIST})

# 生成目标文件需要连接的库名，这里就无需加 -l了，会自动加  main 是目标文件，要和我们生成的可执行文件名一致
target_link_libraries(main muduo_net muduo_base pthread)