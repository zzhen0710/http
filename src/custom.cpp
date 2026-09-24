#include <utility.h>
#include "custom.h"
#include "http.h"

// ============================================================
//  路由定义 + 内部函数（仅本文件使用，加 static）
// ============================================================

// 路由枚举：每个业务一个值
typedef enum {
    R_LOGIN_PAGE,   // GET  /login.html   登录表单页
    R_LOGIN,        // GET  /login        处理登录
    R_ADD_PAGE,     // GET  /add.html     加法表单页
    R_ADD,          // POST /add          处理加法
    R_RESULT_PAGE,  // GET  /result.html  登录结果页
    R_404           // 其他               找不到
} Route;

// 说明：HTTP 里 GET 和 POST 都可能带 query（URL 里 ? 后）和 body（请求正文），
//       只是本项目的业务简单——GET 用 query 传登录参数，POST 用 body 传加法数据，
//       所以这里 do_get 只读 query、do_post 只读 body，没处理更多组合。
// 把 method + url 映射成路由枚举，找不到返回 R_404
static Route match_route(const char* method, const char* url) {
    if (strcmp(method, "GET") == 0) {
        if (strcmp(url, "/login.html") == 0) return R_LOGIN_PAGE;
        if (strcmp(url, "/login")      == 0) return R_LOGIN;
        if (strcmp(url, "/add.html")   == 0) return R_ADD_PAGE;
        if (strcmp(url, "/result.html") == 0) return R_RESULT_PAGE;
    } else if (strcmp(method, "POST") == 0) {
        if (strcmp(url, "/add") == 0) return R_ADD; // 只定义了 POST /add，没定义 GET /add。浏览器地址栏敲 /add 发的是 GET，匹配不到，就 404
    }
    return R_404;   // 其他一律 404
}

// ============================================================
//  对外函数（custom.h 中有声明，http.cpp 的 handle 会调用）
// ============================================================

// 处理 GET 请求：按 url 分发到具体业务
void do_get(int deal_fd, const char* url, const char* query) {
    switch (match_route("GET", url)) {

        case R_LOGIN_PAGE: {
            // 返回登录表单页：读 www/login.html 发回去
            if (send_file(deal_fd, "www/login.html") == -1) {
                send_error(deal_fd, 404);
            }
            break;
        }

        case R_LOGIN: {
            // 从 query 拆出账号密码：query = "username=xx&password=xx"
            // sscanf 按格式串解析：
            //   %63[^&]  → 读到 '&' 为止（最多 63 字符），存进 u
            //   %63s     → 读到空白字符为止（最多 63 字符），存进 p
            // 中间的 "&password=" 是固定要匹配的字面文本
            char u[64] = "", p[64] = "";
            sscanf(query, "username=%63[^&]&password=%63s", u, p);

            char loc[256];
            // 登录规则：账号非空 且 账号 == 密码
            if (strlen(u) > 0 && strcmp(u, p) == 0) {
                // 成功：302 跳到结果页，带 type=ok 和用户名
                snprintf(loc, sizeof(loc), "/result.html?type=ok&name=%s", u);
            } else {
                // 失败：302 跳到结果页，带 type=fail
                snprintf(loc, sizeof(loc), "/result.html?type=fail");
            }
            // 302 重定向：浏览器收到后会自动跳到 loc 指定的地址
            // 这里不返回页面内容，只告诉浏览器"去 /result.html?..."
            send_redirect(deal_fd, loc);
            break;
        }

        case R_ADD_PAGE: {
            // 返回加法表单页：读 www/add.html 发回去
            if (send_file(deal_fd, "www/add.html") == -1) {
                send_error(deal_fd, 404);
            }
            break;
        }

        case R_RESULT_PAGE:
        // 返回结果页：读 www/result.html 发回去
        if (send_file(deal_fd, "www/result.html") == -1)
            send_error(deal_fd, 404);
        break;

        case R_404:
        default:
            // 其他一律 404
            send_error(deal_fd, 404);
            break;
    }
}

// 处理 POST 请求：按 url 分发到具体业务
void do_post(int deal_fd, const char* url, const char* body, int body_len) {
    switch (match_route("POST", url)) {

        case R_ADD: {
            // 从 body 拆出两个数：body = "data1=3&data2=5"
            // sscanf 按格式解析：%d 读整数，遇到 '&' 停
            int a = 0, b = 0;
            sscanf(body, "data1=%d&data2=%d", &a, &b);

            // 拼跳转地址：把两个操作数和结果都带上，供 result.html 显示
            // 形如 /result.html?type=add&a=3&b=5&sum=8
            char loc[256];
            snprintf(loc, sizeof(loc),
                "/result.html?type=add&a=%d&b=%d&sum=%d", a, b, a + b);

            // 302 重定向：浏览器自动跳到 loc，由 result.html 负责显示结果
            send_redirect(deal_fd, loc);
            break;
        }

        case R_404:
        default:
            send_error(deal_fd, 404);
            break;
    }
}