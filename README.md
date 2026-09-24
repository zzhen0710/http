# HTTP Server（极简）

基于 TCP 的极简 HTTP 服务器，支持 GET / POST 路由、静态文件返回、302 重定向、
400 / 404 / 405 错误页。用 `sendfile` 零拷贝发静态文件，多线程（一连接一线程）并发。

## 编译

```bash
cmake -B build -S .
cmake --build build
```

生成可执行文件：`thttpd.out`。

## 运行

```bash
./thttpd.out [端口]        # 默认 8964
```

> 在项目根运行（保证能找到 `www/`）。
> 浏览器用 VSCode 端口转发或 `http://127.0.0.1:8964/...` 访问。

## 路由

| 路由 | 作用 |
|------|------|
| `GET /login.html` | 登录页（静态文件） |
| `GET /login?username=xx&password=xx` | 处理登录（账号=密码即成功，302 跳结果页） |
| `GET /add.html` | 加法页（静态文件） |
| `POST /add` | 处理加法（`data1`/`data2`，302 跳结果页） |
| `GET /result.html?type=...` | 结果页（静态文件 + JS 按参数渲染） |
| 其他 | 404 |

**错误码**：400（请求行畸形）、404（找不到）、405（方法非 GET/POST）。

---

## 文件结构

```
http/
├── CMakeLists.txt
├── README.md
├── thttpd.out          # 可执行文件（构建生成）
├── include/
│   ├── http.h          # HTTP 层接口
│   └── custom.h        # 业务层接口
├── src/
│   ├── main.cpp        # 程序入口：init_server + accept + 线程
│   ├── http.cpp        # HTTP 层：监听、解析、路由、发响应、sendfile
│   └── custom.cpp      # 业务层：do_get / do_post
├── www/                # 静态页面
│   ├── login.html
│   ├── add.html
│   └── result.html
└── build/              # CMake 中间文件（不提交 Git）
```

> `build/` 为 CMake 构建目录，存放中间文件（`.o`、缓存、Makefile 等），
> 不属于源码，已在 `.gitignore` 中排除。
> 运行时必须在项目根（`www/` 相对运行目录）。

---

## 文件业务

### http：HTTP 层

| 函数 | 作用 |
|------|------|
| `init_server` | `socket` → `setsockopt(SO_REUSEADDR)` → `bind` → `listen` |
| `handle` | 一个连接：解析请求行 → 校验方法 → 读 body → 分发 `do_get`/`do_post` |
| `parse_request` | 拆出 method / url / query |
| `skip_header` | 跳请求头、抓 `Content-Length` |
| `get_line` | 逐字符读一行，`\r\n` 归一成 `\n` |
| `send_error` | 发错误响应（400 / 404 / 405） |
| `send_response` | 发正常响应（状态码 + 正文） |
| `send_redirect` | 发 302 重定向（`Location`） |
| `send_file` | 发静态文件（`open`/`fstat` 拿大小 → 拼头 → `sendfile` 零拷贝） |

### custom：业务层

| 函数 | 作用 |
|------|------|
| `do_get` | 按 url 路由：`/login.html`、`/login`、`/add.html`、`/result.html`、404 |
| `do_post` | 按 url 路由：`/add`（处理加法）、404 |

---

## 通信逻辑

**请求处理流程**：

```
accept → 创建线程 → handle:
  1. parse_request   → method / url / query
  2. 校验 method     → 非 GET/POST → 405
  3. skip_header     → 拿 Content-Length
  4. POST 按长度读 body
  5. 分发：GET → do_get、POST → do_post
  6. 业务：读文件 / 处理数据 → send_response / send_file / send_redirect
```

**登录 / 加法结果**：用 **302 重定向**跳 `/result.html?type=...`；
`result.html` 用 **JS 读 URL 参数**渲染成功/失败/结果（静态文件 + 前端渲染）。

---

## 静态文件（www）

| 文件 | 作用 |
|------|------|
| `login.html` | 登录表单（GET `/login`，账号=密码即成功） |
| `add.html` | 加法表单（POST `/add`） |
| `result.html` | 结果页：JS 读 `?type=ok/fail/add` 显示不同内容 |

---

## 技术栈

- **语言 / 标准**：C++11
- **网络**：TCP socket（`socket` / `bind` / `listen` / `accept` / `recv` / `send`）
- **HTTP**：请求行解析、`Content-Length`、状态码、`Location` 重定向、`sendfile` 零拷贝
- **并发**：`pthread`（一连接一线程）
- **前端**：HTML + CSS + JS（静态文件 + URL 参数渲染）
- **构建**：CMake

---

## 版本历史

| 版本 | 说明 |
|------|------|
| Release 1.0 | HTTP 服务器：解析、路由、静态文件、登录/加法、400/404/405 |