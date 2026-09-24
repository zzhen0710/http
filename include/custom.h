#ifndef CUSTOM_H
#define CUSTOM_H

#include "utility.h"

// 处理 GET 请求：url 是路径，query 是查询串
void do_get(int deal_fd, const char* url, const char* query);

// 处理 POST 请求：url 是路径，body 是请求正文，body_len 是正文长度
void do_post(int deal_fd, const char* url, const char* body, int body_len);

#endif