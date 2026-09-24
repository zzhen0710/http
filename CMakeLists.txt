# 规定 CMake 最低版本为 3.0，低于此版本的 CMake 无法解析本文件
cmake_minimum_required(VERSION 3.0)

# 项目名称：thttpd（tiny http daemon，微型 HTTP 服务器）
project(thttpd)

# 指定 C++ 标准为 C++11（用到 nullptr、auto、列表初始化等特性时需要）
set(CMAKE_CXX_STANDARD 11)

# 把 include/ 目录加入头文件搜索路径
# ${CMAKE_SOURCE_DIR} → /home/zhen/01_HTTP，值 = 项目根目录的绝对路径/include拼在后面，指向项目根下的 include/
include_directories(${CMAKE_SOURCE_DIR}/include)

# 声明要生成的可执行文件 thttpd.out，并列出它的所有源文件
# 编译时会把这三个 .cpp 一起编译、链接成 thttpd.out
add_executable(thttpd.out
    src/main.cpp      # 主函数：启动、accept、起线程
    src/http.cpp      # HTTP 层：监听、解析、发响应
    src/custom.cpp    # 业务层：登录、加法、404
)

# set_target_properties( 目标名 PROPERTIES 属性名 值 ... )
# 设置 thttpd.out 这个目标的属性
# RUNTIME_OUTPUT_DIRECTORY 指定"可执行文件生成到哪"
# 默认是当前工作目录（也就是跑 cmake 的目录，通常就是 build/）
# 这里改成项目根目录（${CMAKE_SOURCE_DIR}），让可执行文件和源码放一起
set_target_properties(thttpd.out PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# 链接 pthread 线程库
# 因为用了 pthread_create / pthread_detach 等，必须链接 -lpthread
target_link_libraries(thttpd.out pthread)