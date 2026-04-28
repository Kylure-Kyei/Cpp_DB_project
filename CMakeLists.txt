cmake_minimum_required(VERSION 3.14)
project(MiniDB VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准为 C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g") # 开启警告和调试信息

# --- 源文件定义 ---

# 公共基础库 (成员 A 负责)
set(COMMON_SRC
    #src/common/MyVector.cpp
    #src/common/MyString.cpp
)

# 索引模块 (成员 A 负责 - B+树)
set(INDEX_SRC
    src/index/BPlusTree.cpp
)

# 存储模块 (成员 A 负责 - 表与行)
set(STORAGE_SRC
    src/storage/Table.cpp
    src/storage/Row.cpp
)

# 服务端 (成员 B 负责)
set(SERVER_SRC
    src/server/Server.cpp
    src/server/Network.cpp
)

# 客户端 (成员 B 负责)
set(CLIENT_SRC
    src/client/Client.cpp
)

# --- 可执行文件构建 ---

# 1. 构建服务端可执行文件
add_executable(minidb_server 
    ${COMMON_SRC} 
    ${INDEX_SRC} 
    ${STORAGE_SRC} 
    ${SERVER_SRC}
)

# 测试目标
add_executable(test_all 
    ${COMMON_SRC}
    ${INDEX_SRC}
    src/test/test_all.cpp
)
target_include_directories(test_all PRIVATE src)

include_directories(src/common src/index src/storage)
