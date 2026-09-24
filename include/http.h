#ifndef _HTTP_H_
#define _HTTP_H_

#define BUF_SIZE 4096

// ============================================================
//  http.h —— HTTP 层对外接口
//  职责：监听套接字、解析请求、分发业务、发送响应
//  被 main.cpp（启动/处理连接）和 custom.cpp（发响应）调用
// ============================================================

// 创建监听套接字：socket → bind → listen
// 成功返回监听 fd（list_fd），失败返回 -1
int init_server(int port);

// 处理一个连接：解析请求、校验方法、分发业务
int handle(int deal_fd);

// 发送错误响应（400 / 404 / 405）
void send_error(int deal_fd, int status_code);

// 发送正常响应：状态码 + 正文
void send_response(int deal_fd, int status_code, const char* body);

// 发送 302 重定向：浏览器收到后自动跳到 location 指定的地址
void send_redirect(int deal_fd, const char* location);

// 发送本地文件作为响应：算长度 → 拼头 → 发头 → sendfile 发正文
// 成功返回 0，文件不存在返回 -1
int send_file(int deal_fd, const char* path);

#endif