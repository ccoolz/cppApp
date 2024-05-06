# 基于muduo、json、redis开发的集群通信服务器

[TOC]

## 1.开发环境

> 运行在 Windows主机下的 ubuntu虚拟机
>
> 代码开发过程主要通过 vscode的 remoteSSH插件进行远程连接和开发
>
> 在涉及到需要文件共享时使用 samba创建的共享文件夹



## 2.涉及技术

> 基于 **muduo库**的网络模块开发
>
> 基于 **json**的消息传递，序列化与反序列化
>
> **mysql数据库**表的设计，字符集设置，类 **orm思想**的数据库模型编程
>
> 基于 **redis发布-订阅功能**的**跨服务器通信**设计，基于 hiredis库的 redis编程
>
> 利用 **nginx**服务，.conf 添加 tcp模块，实现多服务器的**负载均衡**
>
> 多线程接收消息的同步处理
>
> **cmake**构建编译环境，shell脚本自动编译运行



## 3.编译配置

> **主体使用 cmake进行自动化编译**
>
> 最外层 /Chat 路径下 CMakeLists.txt
>
> ```cmake
> cmake_minimum_required(VERSION 3.0)
> project(chat)
> # PROJECT_SOURCE_DIR :CMake会自动设置 PROJECT_SOURCE_DIR的默认值为项目根目录的路径（CMakeLists.txt所在的路径）
> # 如果你使用的是子目录 CMakeLists.txt，那么 PROJECT_SOURCE_DIR将会是子目录的路径。
> 
> # 配置编译选项  CXX代表 C++编译器，默认 g++
> set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)
> 
> # 配置可执行文件输出的路径
> set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
> 
> # 配置头文件的搜索路径
> include_directories(${PROJECT_SOURCE_DIR}/include)              # server 和 chat 通用头文件
> include_directories(${PROJECT_SOURCE_DIR}/include/server)       # server 单独使用的头文件
> include_directories(${PROJECT_SOURCE_DIR}/include/server/db)    # mysql 数据库相关头文件
> include_directories(${PROJECT_SOURCE_DIR}/include/server/model) # 封装了数据库操作接口的相关类，使业务面向对象编程，拆分数据模块
> include_directories(${PROJECT_SOURCE_DIR}/include/server/redis) # redis 相关操作类
> include_directories(${PROJECT_SOURCE_DIR}/thirdparty)           # json.hpp
> 
> # 加载子目录    会自动去子指定目录下寻找 CMakeLists文件，继续执行 cmake
> add_subdirectory(src)
> ```
>
> 次外层 /src 路径下 CMakeLists.txt
>
> ```cmake
> *# 去 server子目录寻找 CMakeLists.txt*
> add_subdirectory(server)
> add_subdirectory(client)
> ```
>
> 最内层 /server 路径下 CMakeLists.txt
>
> ```cmake
> # 定义了一个 SRC_LIST变量，包含了指定目录下所有的源文件
> aux_source_directory(. SRC_LIST)
> aux_source_directory(./db DB_LIST)
> aux_source_directory(./model MODEL_LIST)
> aux_source_directory(./redis REDIS_LIST)
> 
> # 指定源文件共同编译生成可执行文件  输出可执行文件的目录已在最外层 cmakelists中指定
> add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST})
> 
> # 指定可执行文件连接时需要依赖的库文件
> target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread)
> ```
>
> 最内层 /client 路径下 CMakeLists.txt
>
> ```cmake
> # 附属源文件目录 定义了一个 SRC_LIST变量，包含了该目录下所有的源文件
> aux_source_directory(. SRC_LIST)
> 
> # 指定生成的可执行文件
> add_executable(ChatClient ${SRC_LIST})
> 
> # 指定可执行文件链接时需要依赖的库
> target_link_libraries(ChatClient pthread)   # 客户端包含两个线程，读线程和写线程
> ```
>
> 自动编译的shell脚本文件 /Chat 路径下 autobuild.sh
>
> ```sh
> #!/bin/bash
> set -x
> 
> rm -rf "$(pwd)/build/"*
> cd "$(pwd)/build" &&
> 	cmake .. &&
> 	make
> ```
>
> 自动运行服务器的shell脚本文件 /Chat 路径下 runserver.sh
>
> ```sh
> #!/bin/bash
> set -x
> 
> cd "$(pwd)/bin" &&
> 	./ChatServer 127.0.0.1 5000
> ```
>
> ​																					**[注意]：运行第二个或以上的服务器需要手动启动**
>
> 自动运行客户端的shell脚本文件 /Chat 路径下 runclient.sh
>
> ```sh
> #!/bin/bash
> set -x
> 
> cd "$(pwd)/bin" &&
> 	./ChatClient 127.0.0.1 8000
> ```



## 4.主体文件

> ---
>
> ![image-20240501130532215](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240501130532215.png)



## 5.数据库表设计

> **chat 数据库**中的所有表如下
>
> ![image-20240501130749512](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240501130749512.png)
>
> ---
>
> **【 all_group 表 】**: 一条记录代表一个群组，属性包括 群id、群名、群组描述，其中，群id自增且作为主键，群名不能重复
>
> ![image-20240501130856198](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240501130856198.png)
>
> 【 **friend 表 】**: 一条记录代表一对朋友，（用户id，朋友id）的联合主键设置使朋友不会重复
>
> ![image-20240501131216355](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240501131216355.png)
>
> **【 group_user 表 】**: 一条记录代表某个群中的某个用户，字段包括 群id、用户id、用户在群中的角色（创建者 | 加入者），其中，群id和用户id作为联合主键确保用户不会在群中重复
>
> ![image-20240501131709523](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240501131709523.png)
>
> **【 offline_message 表 】**: 一条记录代表某用户的收到的一条离线消息，无主键设置
>
> ![image-20240501132214521](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240501132214521.png)
>
> **【 user 表 】**: 一条记录代表一个用户，字段包括 用户id、用户名、密码、在线状态，其中，用户id是自增主键，用户名不可重复，在线状态在（online，offline）中枚举
>
> ![image-20240501132348894](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240501132348894.png)



## 6.功能介绍

> 注册
>
> 登录
>
> 退出登录（注销）
>
> 添加好友
>
> 用户一对一通信
>
> 创建群组
>
> 加入群组
>
> 群组通信
>
> 退出客户端



## 7.运行演示

> **启动两个服务器**
>
> ![image-20240507005124438](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240507005124438.png)
>
> ---
>
> **启动两个客户端，通过nginx将连接请求负载均衡到两个服务器上**
>
> ![image-20240507005246380](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240507005246380.png)
>
> ---
>
> **登录**
>
> ​	登录成功显示当前登录的用户、好友列表和群组列表
>
> ![image-20240507005326902](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240507005326902.png)
>
> ​	系统自动弹出客户端功能指引，用户可以按指引上的格式输入使用对应功能
>
> ![image-20240507005507104](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240507005507104.png)
>
> ---
>
> 仅展示用户间**一对一通信**功能
>
> ![image-20240507005814985](C:\Users\31319\AppData\Roaming\Typora\typora-user-images\image-20240507005814985.png)
>
> 



## 8.Bug与解决方案

> ---
>
> ### ***bug_1:*** 
>
>   ***表现__***
>
> ​    	*若使用 const char\* 接 json.dump().c_str()数据，如*
>
> ​    	*const char \*buf = js.dump().c_str()*
>
> ​    	*vscode 不会报错 | vs会报错 C26815: 指针无关联，因为它指向已销毁的临时实例。*
>
> ​    	*用这种方式接下来的是无效的 const char\* 数据*
>
>   ***原因分析__***
>
> ​    	*.dump()返回的是临时 string 对象，这一行语句结束，string 对象自动销毁*
>
> ​    	*它底层的 char 数组也被销毁，那么 const char\* 指向的地址就失效，造成了悬空指针的问题*
>
>   ***解决方法__***
>
> ​    	*用 std::string 对象去接 .dump()返回的对象临时，相当于构造了一个局部变量，*
>
> ​    	*再用该局部变量进行返回 c_str 等操作*
>
> ​    	*这中间的主要区别：用指针类型指向一个临时变量 or 用临时变量的数据构造一个新的变量*

---

> ### ***bug_2:*** 
>
>   ***表现__***
>
> ​    	*当聊天用户离线时，离线消息会被存储到数据库*
>
> ​    	*当离线消息中包含中文，某些消息插入数据库的操作会失败，某些却会成功，如"你好""还可以吧"可以成功插入，*
>
> ​    	*不过在数据库中是乱码，读到程序中却又显示为中文*
>
> ​    	*mysql_error(conn)打印 Incorrect string value: '\x89","ms...' for column 'message' at row 1*
>
>   ***原因分析__***
>
> ​    	*预计可能是数据库/json/终端字符集中的某个或多个有问题，或不一致导致的*
>
>   ***解决方法__***
>
> ​    	*vscode 右下角编码格式就是默认的 UTF-8* 
>
> ​        	*-- 经测试，改为 GBK结果相同，不是 vscode编码的问题*
>
> ​    	*db.cpp:39行处代码为 mysql_query(conn, "set names utf8");* 
>
> ​        	*-- 改为 set names gbk出现上述错误 Incorrect string value*
>
> ​    	*在 linux 中进入 mysql chat表，执行ALTER TABLE offline_message DEFAULT CHARACTER SET gbk;*
>
> ​        	*-- 经测试，不管 SET gbk 还是 SET utf8，都没有问题*
>
>   	*综上所述*
>
> ​    	*核心原因就是需要在连接数据库后设置进行操作 mysql_query(conn, "set names utf8");  它表现为*
>
> ​    	*character_set_client   | utf8*
>
> ​    	*character_set_connection| utf8*
>
> ​    	*character_set_results  | utf8*
>
> ​    	*而在这之前使用的语句 mysql_query(conn, "set names gbk");这会导致不兼容的问题从而中文数据无法插入数据库*

---

> ### ***bug_3:*** 
>
>   ***表现__***
>
> ​    	*发送多条消息给离线用户，用户上先后只会收到一条消息*
>
>   ***原因分析__***
>
>    	 *这是离线消息表设计上的问题，一开始把表的 userid设为主键，导致一个 userid只会有一条数据被储存*
>
>   ***解决方法__***
>
> ​    	*重新建表，userid不设为主键，设为 not null即可*

---

> ### ***bug_4:***
>
>   ***表现__***
>
> ​    	*creategroup:中文:中文失败*
>
>   ***原因分析__***
>
>    	 *show create table all_group结果如下*
>
> ​      	*| all_group | CREATE TABLE `all_group` (*
>
> ​     	 *`id` int(11) NOT NULL AUTO_INCREMENT COMMENT '组id',*
>
> ​    	  *`groupname` varchar(50) CHARACTER SET latin1 NOT NULL COMMENT '组名称',*
>
>    	   *`groupdesc` varchar(200) CHARACTER SET latin1 DEFAULT '' COMMENT '组功能描述',*
>
> ​    	 *PRIMARY KEY (`id`),*
>
> ​     	 *UNIQUE KEY `groupname` (`groupname`)*
>
> ​      	*) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8* 
>
> ​    	*发现表的默认字符集是 utf8 但`groupname``groupdesc`的字符集都是 latin1*
>
>   ***解决方法__***
>
> ​    	*mysql> ALTER TABLE all_group MODIFY groupname varchar(50) CHARACTER SET utf8 NOT NULL;*
>
> ​    	*mysql> ALTER TABLE all_group MODIFY groupdesc varchar(200) CHARACTER SET utf8 DEFAULT '';*

---

> ### ***bug_5:***
>
>   ***表现__***
>
> ​    	*比如 json解析错误等原因导致服务器程序异常结束时，已登录的用户的状态仍是 online，*
>
> ​    	*导致客户端用户的下次登录无法成功*

---

> ### *bug_6:*
>
>   ***表现__***
>
> ​    	*用户登录成功后打印了用户所在群组，却没按预想打印群组中的成员信息*
>
>   ***原因分析__***
>
> ​    	*本文件的 login函数登录成功后获取组和组成员中有一条语句*
>
> ​    	*std::vector<GroupUser> group_users = group.getUsers(); // 返回引用，相当于给 group对象的成员 users取别名*
>
> ​    	*group.getUsers()虽然返回引用，但接它的变量也需要是引用。否则就不是在操作目标 group的 users成员*
>
>   ***解决方法__***   
>
> ​    	*用引用变量来接 std::vector<GroupUser> &group_users = group.getUsers();*

---

> ### ***bug_7:***
>
>   ***表现__***
>
> ​    	*用户登出后再登录，有时输入密码后却无响应，没有按代码逻辑进行下一步*
>
>   ***原因分析__***
>
> ​    	*从再次登录后的代码逻辑进行分析：用户输入密码 -> 客户端向服务器发送登录请求消息 -> 服务器回复消息(成功或失败都有响应)*
>
> ​    	*那么就是在上述过程中发生了问题，核心问题在于，服务器接收到了消息，为什么不回复消息呢？*
>
> ​    	*经过分析，我们发现原因来自子线程，在上次登录成功后，子线程开始读取服务器发送的数据，*
>
> ​    	*而登录函数所在的主线程也在等待服务器发送的返回数据，这时，若数据被子线程读走了，主线程就接收不到该数据，阻塞在了 recv()处*
>
> ​    	*这个 bug是多线程问题，无法简单通过调试排出来，需要对程序了解熟悉过程，思考出来*
>
>   ***解决方法__***
>
> ​    ***（1）***
>
> ​      	*设置一个变量 bool is_online，设置子线程循环接收信息的循环条件为 is_online*
>
> ​      	*这样退出登录后就无法继续这个循环，子线程函数执行结束，退出子线程*
>
> ​      	*此时出现频率已大大减少，但还会有上述错误情况，应该是子线程此时阻塞在 recv，在循环体内，还是会可能读走数据*
>
> ​      	*那么就要想办法解决这个登录数据被读走的情况*
>
> ​        	*（目前想到的解决方案是 子线程的 登录中还应该有一个发送回应消息给服务器的部分*
>
> ​       	 *当服务器发送的登录确认消息后开始 recv（这里应该用 muduo的接收 api）子线程 login()发送的登录确认消息*
>
> ​        	*若一段时间后还没有收到确认消息，说明发送的登录响应消息被客户端子线程读走，此时子线程函数进入判断，发现用户已下线，子线程结束*
>
> ​        	*那么再发送一次登录响应消息就一定会被客户端主线程的登录程序接收。这个思路类似三次握手，客户端发登录消息 - 服务端发登录确认消息 - 客	户端发确认登录成功消息 的过程*
>
> ​        	*还想到一种方法，子线程对收到的消息进行判断时，对收到消息为登录响应消息的情况进行处理（未完待续），先用这种方法确认是否是这个原因*
>
> ​       	 *括号内的想法未采用）*
>
> ​    ***（2）***
>
> ​     	 *目前的处理方式是 ，在（1）的基础上，服务器连续发两次登录消息*
>
> ​     	 *若第一次被子线程读走，那么子线程也不会继续读第二次了，break出去，子线程结束，线程登录后开启另一个子线程。主线程读走第二次成功登录*
>
> ​      	*若第一次被主线程读走，主线程接下来的函数逻辑中没有读消息这一条，子线程一定会读到第二条，break出去，这个子线程结束，主线程登录后开启另一个子线程*
>
> ​     	 更正，不应该 break
>
> ​      	主线程先接收到登录确认，先登录，然后开启子线程，子线程接收到第二条登录确认，若 break出去，无法进行接下来的正常接收聊天消息*
>
> ​      	若什么也不做，分类讨论
>
> ​       	 *// （1）主线程接收到第一条，登录成功，登录后开启子线程，子线程接收到第二条，此时登录状态变量为 true，进入下一轮循环，子线程可以正				常接收消息*
>
> ​        	*// （2）子线程接收到第一条，等待一小小段可以接受的时间，此时主线程接收到第二条登录确认，登录成功，登录状态变量立即被置 false，*
>
> ​          			子线程无法继续接受消息从而执行结束，主线程会另开一条线程接受消息*