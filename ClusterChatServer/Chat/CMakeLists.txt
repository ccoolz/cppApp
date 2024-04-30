cmake_minimum_required(VERSION 3.0)
project(chat)
# PROJECT_SOURCE_DIR :CMake会自动设置 PROJECT_SOURCE_DIR的默认值为项目根目录的路径（CMakeLists.txt所在的路径）
# 如果你使用的是子目录 CMakeLists.txt，那么 PROJECT_SOURCE_DIR将会是子目录的路径。

# 配置编译选项  CXX代表 C++编译器，默认 g++
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

# 配置可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 配置头文件的搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)              # server 和 chat 通用头文件
include_directories(${PROJECT_SOURCE_DIR}/include/server)       # server 单独使用的头文件
include_directories(${PROJECT_SOURCE_DIR}/include/server/db)    # mysql 数据库相关头文件
include_directories(${PROJECT_SOURCE_DIR}/include/server/model) # 封装了数据库操作接口的相关类，使业务面向对象编程，拆分数据模块
include_directories(${PROJECT_SOURCE_DIR}/include/server/redis) # redis 相关操作类
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)           # json.hpp

# 加载子目录    会自动去子指定目录下寻找 CMakeLists文件，继续执行 cmake
add_subdirectory(src)