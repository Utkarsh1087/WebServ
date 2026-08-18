# WebServ

A high-performance, low-level **HTTP/1.1 Web Server** built from scratch in C++ using the raw Windows Socket API (Winsock2). 

WebServ is designed to demonstrate core systems programming, computer networking, and OS-level primitives. Rather than relying on high-level networking libraries (like Node.js `net` or Boost.Asio), it directly communicates with OS sockets, implements custom HTTP parsers/routers, and handles concurrency using non-blocking I/O multiplexing.

---

## 🚀 Key Features

* **TCP/IP Socket Management:** Handcrafted socket lifecycle (`WSAStartup`, `socket`, `bind`, `listen`, `accept`, `closesocket`).
* **Non-Blocking I/O Multiplexing (Level 5):** Employs a single-threaded event loop utilizing the `select()` system call, allowing the server to handle multiple concurrent client connections without spawning overhead threads.
* **Custom HTTP/1.1 Parser (Level 2):** Parses raw byte streams, isolates the request line, maps case-insensitive HTTP headers, and validates payload sizes.
* **Dynamic API Routing (Level 3):** Offers a dynamic routing engine that maps path-method combinations (e.g., `GET /api/health`, `POST /api/echo`) to dynamic callback handlers.
* **Static File Serving (Level 1):** Resolves URIs to static files in a `./public` directory, automatically mapping correct MIME types (`.html`, `.css`, `.js`, `.png`, `.jpg`, `.ico`, etc.) and writing response headers.
* **Configuration Loader (Level 4):** Parses a `webserv.conf` file to dynamically configure port, document root, home index file, and upload size boundaries.
* **Robust Security (Level 6):**
  * **Path Traversal Protection:** Sanity checks paths to reject execution attempts escaping the public directory via `../`.
  * **Inactivity Timeouts:** Automatically drops inactive connections after 10 seconds of silence to prevent denial-of-service (DoS) resource exhaustion.
  * **Size Limiters:** Rejects requests exceeding the configured `max_body_size` with `413 Payload Too Large`.

---

## 🏛️ System Architecture

```
                     +---------------------------+
                     |  Client (Browser / Curl)  |
                     +-------------+-------------+
                                   |
                                   | (TCP Streams)
                                   v
                     +-------------+-------------+
                     |   Non-blocking select()   |
                     |         Event Loop        |
                     +-------------+-------------+
                                   |
                       (Accept / Read Activities)
                                   v
                     +-------------+-------------+
                     |    Connection Manager     |
                     |      (m_clients pool)     |
                     +-------------+-------------+
                                   |
                       (Header & Body Buffering)
                                   v
                     +-------------+-------------+
                     |      HTTP/1.1 Parser      |
                     +-------------+-------------+
                                   |
                           (Security Check)
                                   v
                     +-------------+-------------+
                     |     Routing Engine        |
                     +-------+-------------+-----+
                             |             |
                (Route Match) |             | (Fallback)
                             v             v
               +-------------+---+     +---+-------------+
               |  Dynamic API    |     |  Static File    |
               |  Handlers       |     |  Server         |
               +-------------+---+     +---+-------------+
                             |             |
                             +------+------+
                                    |
                                    v
                     +--------------+------------+
                     |    HTTP/1.1 Response      |
                     +---------------------------+
```

---

## 📂 Project Structure

```text
WebServ/
├── public/                 # Document root folder for static files
│   ├── index.html          # Dashboard page
│   ├── style.css           # Glassmorphic stylesheet
│   └── 404.html            # Custom resource not found fallback
├── src/                    # C++ source code
│   ├── main.cpp            # Entry point & API route registration
│   ├── Server.h / .cpp     # TCP socket pool & select() event loop
│   ├── HttpParser.h / .cpp # HTTP request extraction and sanitization
│   ├── Router.h / .cpp     # URI mapping and HTTP response generator
│   └── Config.h / .cpp     # Parse configuration keys from webserv.conf
├── tests/                  # Integration tests
│   └── test_server.ps1     # Automated PowerShell script
├── webserv.conf            # Main config settings file
└── CMakeLists.txt          # Cross-platform build script
```

---

## 🛠️ Build & Run Instructions

### Prerequisites
* Windows OS
* A C++ compiler supporting C++17 (GCC/MinGW, Clang, or Visual Studio MSVC)

### Compile with MinGW (g++)
Run this command from your terminal:
```bash
g++ -std=c++17 src/*.cpp -o webserv.exe -lws2_32
```

### Compile with MSVC (cl.exe)
From a Developer Command Prompt:
```cmd
cl /EHsc /std:c++17 src/*.cpp /Fe:webserv.exe ws2_32.lib
```

### Compile with CMake
If using Visual Studio or VS Code with CMake:
1. Open the project folder.
2. The IDE will auto-configure using the `CMakeLists.txt` file.
3. Select **Build All**.

### Run the Server
Simply execute the compiled binary. By default, it loads `webserv.conf`.
```bash
.\webserv.exe
```
Or specify a manual port override:
```bash
.\webserv.exe 9000
```
Or specify a custom configuration file:
```bash
.\webserv.exe custom.conf
```

---

## 🧪 Verification & Testing

While the server is running, you can verify it in two ways:

### 1. Browser
Navigate to [http://127.0.0.1:8080](http://127.0.0.1:8080) to see the glassmorphic server dashboard.

### 2. Automated Integration Test Script
Open a separate terminal window and execute:
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\tests\test_server.ps1
```
The script runs test assertions covering:
* **GET/**: Asserts correct index serving (`200 OK`).
* **GET/style.css**: Asserts stylesheet delivery with MIME verification.
* **GET/invalid**: Asserts redirect to custom `404.html` pages.
* **GET/api/health**: Checks dynamic route outputs.
* **POST/api/echo**: Posts string payloads and checks content accumulation.
* **HEAD/api/health**: Verifies that headers are sent but body contents are omitted.
* **GET/../main.cpp**: Checks path traversal blockade (`400 Bad Request`).
* **PUT/index.html**: Checks verb validation (`405 Method Not Allowed`).
* **Idle connections**: Connects to the socket and waits 10s without sending data to verify inactivity timeouts (`408 Request Timeout`).