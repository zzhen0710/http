#include "http.h"
#include "custom.h"

#include <cstdio>           // printf, fprintf, snprintf, sscanf, perror
#include <cstring>          // strcmp, strcpy, strlen, strchr, strncasecmp
#include <cstdlib>          // atoi
#include <unistd.h>         // close, read
#include <fcntl.h>          // open, O_RDONLY

#include <sys/socket.h>     // socket, bind, listen, accept, recv, send, setsockopt
#include <netinet/in.h>     // sockaddr_in, INADDR_ANY, htons
#include <sys/stat.h>       // fstat, struct stat
#include <sys/sendfile.h>   // sendfile

// ============================================================
//  内部函数（仅本文件使用，加 static，其他文件不可见）
//  放在最前，供下面的 handle 等调用
// ============================================================

// 自定义仅本文件使用：处理一行http消息，修改buf为修改后的字符串（视'\r'/"\r\n"为'\n'，并'\n'后加'\0'）
static int get_line(int deal_fd, char* buf) {
    char ch = '\0'; // 记录读取的单个字符
    int i = 0; // 读指针，从下标0开始

    // 对单行，开始逐个字符处理
    while(i < BUF_SIZE && ch != '\n') {
        int ret = recv(deal_fd, &ch, 1, 0); // 每次读取一个字符
        if (ret > 0 && ch == '\r') {
            ret = recv(deal_fd, &ch, 1, MSG_PEEK); // peek偷看，只读不取，用于分支条件判断
            if (ret > 0 && ch == '\n') {
                recv(deal_fd, &ch, 1, 0); // 直接读出回车，由循环退出
            } else {
                ch = '\n'; // 不是回车，改为回车，由循环退出
            }
        }
        buf[i++] = ch; // 读取的字符放入缓冲区
    }
    buf[i] = '\0'; // 将字符串补充完整

    return i; // i = 1代表读的是首部尾行
}

// 定义清空（跳过）首部，返回 Content-Length（没有则返回 0）
static int skip_header(int sock)
{
    char buf[BUF_SIZE] = ""; // 读取消息的容器
    int ret = 0;             // 记录读取的个数
    int content_len = 0;     // POST 正文长度

    do {
        ret = get_line(sock, buf); // 循环将每一行数据全部读取出来

        // 顺便抓 Content-Length，供后面读 body 用
        if (strncasecmp(buf, "Content-Length:", 15) == 0) {
            content_len = atoi(buf + 15);
        }
    } while (ret != 1); // do-while先读再判断退出

    return content_len;
}

// 处理请求行，修改对应参数为处理后字符串（无字段置为空串'\0'）
static void parse_request(int deal_fd, char* method, char* url, char* query) {
    char buf[BUF_SIZE] = ""; // 消息缓冲区
    int ret = get_line(deal_fd, buf);
    if (ret <= 0) {
        method[0] = '\0';
        url[0]    = '\0';
        query[0] = '\0';
        return;
    }
    // 用 sscanf 按 "方法 路径" 2个字段拆分，同时限制长度
    if (sscanf(buf, "%15s %4095s", method, url) != 2) {
        method[0] = '\0';
        url[0]    = '\0';
        query[0] = '\0';
        return;
    }

    // 拆查询串：? 前是路径，? 后是 query
    query[0] = '\0';     // 先把query字符串置空
    char* qmark = strchr(url, '?');  // 在url里查找字符'?'，找到返回指向'?'的指针
    if (qmark) {         // 如果找到了 ?
        *qmark = '\0';              // url 截断成纯路径
        strcpy(query, qmark + 1);   // query 存参数部分
    }
}

// ============================================================
//  对外函数（http.h 中有声明，main.cpp / custom.cpp 会调用）
// ============================================================

// 创建监听套接字：socket → bind → listen
// 成功返回监听 fd（list_fd），失败返回 -1
int init_server(int port) {
    // 1. 创建 TCP 套接字
    int list_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (list_fd == -1) {
        perror("socket error");
        return -1;
    }

    // 2. 设置 SO_REUSEADDR，允许端口复用，该属性需要再绑定之前设置
    int opt = 1;
    if (setsockopt(list_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt error");
        close(list_fd);   // 前面创建的资源要回收，避免 fd 泄漏
        return -1;
    }

    // 3. 准备本机地址结构：用列表初始化直接填入各成员
    struct sockaddr_in local_info = {
        AF_INET,      
        htons(port),
        { INADDR_ANY }, // 这里也是结构体，显式双重列表初始化
    };

    // 4. 绑定：把套接字和上面的地址（IP + 端口）关联起来
    if (bind(list_fd, (struct sockaddr*)&local_info, sizeof(local_info)) == -1) {
        perror("bind error");
        close(list_fd);
        return -1;
    }

    // 5. 监听：把套接字变成被动监听状态，等待客户端连接
    //    backlog = 128，表示已完成连接、等待 accept 的队列最大长度
    if (listen(list_fd, 128) == -1) {
        perror("listen error");
        close(list_fd);
        return -1;
    }

    printf(">>> server init success, listening on port %d, list_fd = %d\n", port, list_fd);

    return list_fd;
}

// 发送错误响应：函数内部直接根据状态码给一句简单文案
void send_error(int deal_fd, int status_code) {
    // 1. 根据状态码选一句说明
    const char* text;
    switch (status_code) {
        case 400: text = "400 Bad Request";          break;  // 请求格式错误（如请求行畸形）
        case 404: text = "404 Not Found";            break;  // 请求的资源不存在
        case 405: text = "405 Method Not Allowed";   break;  // 请求方法不被允许（非 GET/POST）
        default:  text = "Error";                    break;  // 其他未知状态码，兜底
    }

    // 2. 拼一段简单 HTML 正文
    char body[256];
    snprintf(body, sizeof(body), "<h1>%s</h1>", text);

    // 3. 拼响应：头 + 空行 + 正文，一次 send 发完
    char resp[512];
    int n = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %d Error\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n" // 诉浏览器响应完关连接，浏览器可能等复用，卡住
        "\r\n"
        "%s",
        status_code, (int)strlen(body), body);

    send(deal_fd, resp, n, 0);
}

// 发送正常响应：状态码 + 正文（由调用者提供）
void send_response(int deal_fd, int status_code, const char* body) {
    char resp[BUF_SIZE];
    int n = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status_code, (int)strlen(body), body);

    send(deal_fd, resp, n, 0);
}

// 发送 302 重定向：浏览器收到后自动跳到 location 指定的地址
void send_redirect(int deal_fd, const char* location) {
    char resp[512];
    int n = snprintf(resp, sizeof(resp),
        "HTTP/1.1 302 Found\r\n"       // 状态行：302 表示临时重定向
        "Location: %s\r\n"             // 关键：告诉浏览器跳到哪
        "Content-Length: 0\r\n"        // 重定向没有正文
        "Connection: close\r\n"
        "\r\n",
        location);

    send(deal_fd, resp, n, 0);
}

// 发送本地文件作为响应：算长度 → 拼头 → 发头 → sendfile 发正文
// 成功返回 0，文件不存在返回 -1
int send_file(int deal_fd, const char* path) {
    // 1. 打开文件（只读）
    int file_fd = open(path, O_RDONLY);
    if (file_fd == -1) return -1;

    // 2. 拿文件大小，用来算 Content-Length
    struct stat st;
    if (fstat(file_fd, &st) == -1) {
        close(file_fd);
        return -1;
    }
    long size = st.st_size;

    // 3. 拼响应头（Content-Length 就是文件大小）
    char head[512];
    int n = snprintf(head, sizeof(head),
        "HTTP/1.1 200 OK\r\n"                        // 状态行：协议版本 + 状态码 + 描述
        "Content-Type: text/html; charset=utf-8\r\n" // 正文类型：HTML，UTF-8 编码（防中文乱码）
        "Content-Length: %ld\r\n"                    // 正文长度：发文件时就是文件字节数
        "Connection: close\r\n"                      // 响应完关闭连接，不复用
        "\r\n",                                      // 空行：头部与正文的分界，必须有
        size);

    // 4. 先单独发响应头：sendfile 只从文件 fd 搬字节，不接受字符串，头和正文没法拼一起发
    send(deal_fd, head, n, 0);

    // 5. 再用 sendfile 发正文（零拷贝），循环防部分发送
    off_t offset = 0;
    while (offset < size) {
        ssize_t sent = sendfile(deal_fd, file_fd, &offset, size - offset);
        if (sent <= 0) break;
        offset += sent;
    }

    // 6. 关闭文件
    close(file_fd);
    return 0;
}

// 通过请求行内容，分支选择具体业务
int handle(int deal_fd) {
    #if DEBUG // 如果在编译时附加定义DEBUG选项，启用
    char peek_buf[BUF_SIZE] = ""; // 控制台瞥一眼，输出发来的消息
    recv(deal_fd, peek_buf, BUF_SIZE, MSG_PEEK);

    printf("-------------------------------------------------\n");
    printf("%s\n", peek_buf);
    printf("-------------------------------------------------\n");
    #endif

    char method[16] = "";           // 储存方法字段
    char url[BUF_SIZE] = "";        // 储存url字段
    char query[BUF_SIZE] = "";      // 储存查询字段
    parse_request(deal_fd, method, url, query);     // 处理得到三部分内容

    // 解析失败：method 为空，说明请求行有问题
    if (method[0] == '\0') {
        fprintf(stderr, "parse_request error, fd = %d\n", deal_fd);
        send_error(deal_fd, 400);   // 400 Bad Request
        return -1;
    }

    // 判断请求方法是GET请求还是POST请求
    if (strcmp(method, "GET") != 0 && strcmp(method, "POST") != 0) {
        // 说明该请求方法既不是GET请求也不是POST请求
        fprintf(stderr, "method not allowed: %s\n", method);
        send_error(deal_fd, 405);   // 405 Method Not Allowed
        return -1;
    }

    // 跳过请求头，拿到 Content-Length
    int content_len = skip_header(deal_fd);

    // POST：按 Content-Length 读 body 正文部分
    char body[BUF_SIZE] = "";
    int body_len = 0;
    if (strcmp(method, "POST") == 0 && content_len > 0 && content_len < BUF_SIZE) {
        body_len = recv(deal_fd, body, content_len, 0);
        if (body_len > 0) body[body_len] = '\0';
    }

    // 按方法分支处理业务
    if (strcmp(method, "GET") == 0) {
        do_get(deal_fd, url, query);
    } else { // POST
        do_post(deal_fd, url, body, body_len);
    }

    return 0;
}