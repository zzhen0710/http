#include "http.h"

#include <cstdio>       // printf, fprintf, perror
#include <cstdlib>      // atoi
#include <cstring>      // strerror
#include <cstdint>      // intptr_t

#include <unistd.h>     // close
#include <pthread.h>    // pthread_create, pthread_detach, pthread_t
#include <sys/socket.h> // accept
#include <netinet/in.h> // sockaddr_in, ntohs
#include <arpa/inet.h>  // inet_ntoa

void* do_business(void* arg) {
    int deal_fd = (int)(intptr_t)arg; // 指针到整型必须显示强转

    // 调用封装的处理函数
    int ret = handle(deal_fd);
    if (ret == -1) {
        fprintf(stderr, "handle error, fd == %d\n", deal_fd);
    }
    close(deal_fd); // 统一用fd句柄关闭该信息处理套接字
    
    return NULL;
}

int main(int argc, const char* argv[]) {
    int port = 8964;
    // 如有手动输入端口号，字符串转整数后返回只修改port
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    // 初始化服务器——创建套接字、绑定、监听
    int list_fd = init_server(port);
    if (list_fd == -1) {
        fprintf(stderr, "init_server error, port == %d\n", port);
        return -1;
    }

    // 循环接收客户端请求，多线程并发处理多客户请求
    struct sockaddr_in cli_info; // 套接字地址结构体，用于接收客户端地址信息
    socklen_t info_len = sizeof(cli_info);
    
    while (true) {
        // 表单fd监听消息，创建用于后续处理信息交换的fd
        int deal_fd = accept(list_fd, (struct sockaddr*)&cli_info, &info_len);
        if (deal_fd == -1) {
            perror("accept error");
            continue;           // 继续等下一个连接，避免一次偶发监听失败关闭整个服务器
        }
        printf(">>> accept success: [IP/Port] = [%s/%d]\n",
                inet_ntoa(cli_info.sin_addr), ntohs(cli_info.sin_port));

        // 新建副线程并发处理消息收发业务
        pthread_t thread_id;    // 线程 ID（pthread_t 是不透明类型，无需初始化）
        int ret = pthread_create(&thread_id, NULL, do_business, (void*)(intptr_t)deal_fd);
        if (ret != 0) { // 传递deal_fd给线程函数
            // accept 已成功，cli_info 有效，可以打 IP/端口
            fprintf(stderr, "pthread_create error: %s (errno=%d) | client=[%s:%d] fd=%d\n",
                strerror(ret), ret, inet_ntoa(cli_info.sin_addr), ntohs(cli_info.sin_port),
                deal_fd);
            close(deal_fd);     // 没成功交给线程，自己关掉
            continue;
        }
        // 线程设置为分离态，系统自动收回防止阻塞
        pthread_detach(thread_id);
    }
    // 用fd句柄关闭该监听套接字
    close(list_fd);

    return 0;
}