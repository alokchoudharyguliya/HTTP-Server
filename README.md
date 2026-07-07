# HTTP Server — Project Documentation

A learning-oriented **C++ HTTP/1.1 static file server** built from scratch using POSIX sockets, with modular design, concurrency via a thread pool, and structured logging.

---

## 1. Project Overview

### DEMO

![Demo Video](ScreenRecording2026-07-08at12.05.36AM-ezgif.com-video-to-gif-converter.gif)

This project implements a minimal but functional HTTP server that:

- Listens on a TCP port (default **8001**)
- Accepts incoming client connections
- Parses HTTP/1.1 GET requests
- Serves static files from the `public/` directory
- Returns proper status codes, MIME types, and `Content-Length`
- Handles multiple clients concurrently using a **thread pool**
- Logs events and requests with timestamps, levels, status, size, and latency

It was evolved from a single-file prototype into a multi-module C++17 codebase, demonstrating networking, HTTP basics, RAII, and concurrent programming.

### Recent improvements

- **Logger** — level-based, mutex-protected logging across all modules
- **`\r\n\r\n` framing** — incremental request reads until headers are complete
- **`SO_REUSEADDR`** — immediate port reuse on server restart

---

## 2. Architecture

```
┌─────────────-┐     TCP      ┌──────────────────────────────────────────┐
│   Client     │ ──────────►  │              HTTPServer                  │
│(curl/browser)|              │  accept loop → ThreadPool → handleClient │
└─────────────-┘              └─────┬───-────────────────────────────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
              HTTPParser      FileServer      MimeResolver
                    │               │               │
                    ▼               ▼               ▼
              HTTPRequest      read file      Content-Type
                    │               │
                    ▼               ▼
              HTTPResponse ◄── body + headers
                    │
                    ▼
              Socket::send()  →  client
                    │
                    ▼
                 Logger (mutex-protected)
```

### Request lifecycle

1. **Main thread** blocks on `listener_.accept()` waiting for a client.
2. Accepted `Socket` is moved into a **packaged task** and submitted to the **ThreadPool** (8 workers).
3. Worker runs `handleClient()`:
  - `recv()` into a buffer, appending to `requestBuffer` until `\r\n\r\n` is found
  - `HTTPParser` parses method, path, version, headers
  - Map path → file under `public/`
  - `FileServer` reads file or returns 404
  - `MimeResolver` sets `Content-Type`
  - `HTTPResponse::serialize()` builds raw HTTP response
  - `Socket::send()` writes response to client
  - `Logger::access()` records the transaction
4. Loop continues for keep-alive (multiple requests per connection until client closes).

---

## 3. Directory Structure

```
http-server/
├── CMakeLists.txt          # Build configuration (C++17)
├── include/                # Public headers
│   ├── HTTPServer.h        # Server orchestration
│   ├── Socket.h            # TCP socket RAII wrapper
│   ├── HTTPRequest.h       # Request data model
│   ├── HTTPResponse.h      # Response data model
│   ├── HTTPParser.h        # Request parser
│   ├── FileServer.h        # Static file I/O
│   ├── MimeResolver.h      # Extension → MIME type
│   ├── Logger.h            # Thread-safe structured logging
│   └── ThreadPool.h        # Worker thread pool
├── src/                    # Implementations
│   ├── main.cpp            # Entry point
│   ├── HTTPServer.cpp
│   ├── Socket.cpp
│   ├── HTTPParser.cpp
│   ├── HTTPResponse.cpp
│   ├── FileServer.cpp
│   ├── MimeResolver.cpp
│   ├── Logger.cpp
│   └── ThreadPool.cpp
├── public/                 # Web root (served files)
│   ├── index.html
│   ├── abc.css
│   └── file.jpeg
└── test/
    └── load_test.sh        # Concurrent load testing script
```

---

## 4. Components

### 4.1 `Socket` — TCP abstraction (RAII)

Wraps POSIX socket APIs with resource-safe ownership.

| Method               | POSIX equivalent              | Purpose                                  |
| -------------------- | ----------------------------- | ---------------------------------------- |
| `create()`           | `socket()`                    | Create TCP socket                        |
| `bind(port)`         | `bind()`                      | Bind to port                             |
| `listen()`           | `listen()`                    | Start listening                          |
| `accept()`           | `accept()`                    | Accept client connection                 |
| `receive()`          | `recv()`                      | Read bytes                               |
| `send()`             | `send()`                      | Write bytes                              |
| `close()`            | `close()`                     | Close FD safely                          |
| `setReuseAddress()`  | `setsockopt(SO_REUSEADDR)`    | Allow immediate port reuse after restart |

**`SO_REUSEADDR`**

Before `bind()`, the server calls `setReuseAddress(true)`, which sets `SO_REUSEADDR` via `setsockopt()`. This lets the process re-bind to the same port immediately after restart, instead of waiting for the OS to release the socket in `TIME_WAIT`.

```cpp
void Socket::setReuseAddress(bool enable) {
  int opt = enable ? 1 : 0;
  ::setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}
```

`bind()` enables this by default before binding to the port.

**Concepts:**

- **RAII** — destructor closes the FD
- **Move-only type** — copy deleted; move transfers ownership
- **File descriptors** — integer handles to OS resources
- **`SO_REUSEADDR`** — socket option for fast server restarts during development

---

### 4.2 `HTTPRequest` / `HTTPResponse` — Data models

**HTTPRequest** holds:

- `method`, `path`, `version`
- `headers` (`unordered_map`)
- `body` (reserved for future POST support)

**HTTPResponse** holds:

- `statusCode`, `reasonPhrase`
- `headers`, `body`
- `serialize()` — builds wire-format HTTP/1.1 response

Example serialized output:

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 225

<!DOCTYPE html>...
```

---

### 4.3 `HTTPParser`

Parses a raw request string into `HTTPRequest`:

1. Reads request line: `GET /path HTTP/1.1`
2. Reads header lines until blank line
3. Splits `Key: Value` pairs

**Concepts:** string streaming (`istringstream`), HTTP message format, header parsing.

---

### 4.4 `FileServer`

Serves files from disk:

- `exists(path)` — `std::filesystem::exists`
- `readFile(path)` — reads entire file into `std::string`

Path mapping in `HTTPServer`:

- `/` → `public/index.html`
- `/abc.css` → `public/abc.css`

---

### 4.5 `MimeResolver`

Maps file extensions to MIME types for the `Content-Type` header:

| Extension       | MIME Type                  |
| --------------- | -------------------------- |
| `.html`         | `text/html`                |
| `.css`          | `text/css`                 |
| `.js`           | `text/javascript`          |
| `.jpeg`, `.jpg` | `image/jpeg`               |
| `.png`          | `image/png`                |
| `.txt`          | `text/plain`               |
| other           | `application/octet-stream` |

---

### 4.6 `Logger`

Thread-safe, level-based logging used across the server (`HTTPServer`, `Socket`, `ThreadPool`, `FileServer`, `HTTPParser`).

| Method     | Purpose                                                       |
| ---------- | ------------------------------------------------------------- |
| `info()`   | Startup config, server state                                  |
| `debug()`  | Connection lifecycle, parsed requests, socket FDs             |
| `warning()`| Recoverable issues (404, thread pool stopped)                 |
| `error()`  | Failures (`bind`, `accept`, `send`, task exceptions)          |
| `access()` | Per-request summary (method, path, status, size, latency)   |

All output goes to **stderr** with a timestamped prefix and is protected by a static `std::mutex`.

**Example output:**

```
[2026-07-07 23:26:00] [INFO] Listening on port 8001
[2026-07-07 23:26:05] [DEBUG] Client Connected
[2026-07-07 23:26:05] [DEBUG] Request: GET /file.jpeg HTTP/1.1
[2026-07-07 23:26:05] [ACCESS] GET /file.jpeg->200	 | 26363 B  | 2
[2026-07-07 23:26:05] [WARNING] File not found: public/missing.html
[2026-07-07 23:26:05] [ERROR] Failed to accept client: Too many open files
```

**Concepts:**

- **Log levels** — separate operational noise from errors and access lines
- **Data races** — multiple threads writing to the console
- `std::mutex` + `lock_guard` — mutual exclusion
- **Static members** — `std::mutex Logger::mutex_;` defined in `Logger.cpp`

---

### 4.7 `ThreadPool`

Fixed pool of worker threads (8) processing a task queue.

| Piece                        | Role                                            |
| ---------------------------- | ----------------------------------------------- |
| `std::queue<Task>`           | Pending work                                    |
| `std::mutex`                 | Protects queue                                  |
| `std::condition_variable`    | Workers wait for tasks                          |
| `std::packaged_task<void()>` | Move-only tasks (works with move-only `Socket`) |

**Concepts:**

- Producer–consumer pattern
- `condition_variable::wait` with predicate
- Move semantics with non-copyable resources
- Graceful shutdown in destructor (`stop_`, `notify_all`, `join`)

---

### 4.8 `HTTPServer`

Central orchestrator:

- Owns listener socket, parser, file server, thread pool
- Accept loop on main thread
- Offloads per-client work to thread pool

```cpp
HTTPServer::HTTPServer(int port, const std::string& root)
    : port_(port), threadPool_(8), documentRoot_(root) {}
```

---

### 4.9 Request buffering (`\r\n\r\n` header detection)

HTTP headers end with a blank line: `\r\n\r\n`. The server no longer assumes a single `recv()` returns a complete request. Instead, `handleClient()` accumulates bytes into `requestBuffer` until that delimiter is found:

```cpp
ssize_t headerEnd = requestBuffer.find("\r\n\r\n");
while (headerEnd == std::string::npos) {
  ssize_t bytesReceived = client.receive(buffer, sizeof(buffer) - 1);
  if (bytesReceived <= 0)
    return;  // client disconnected
  requestBuffer.append(buffer, bytesReceived);
  headerEnd = requestBuffer.find("\r\n\r\n");  // re-check after each read
}
HTTPRequest request = parser_.parse(requestBuffer);
```

**Why this matters:**

- A single `recv()` may return partial headers; without re-checking `\r\n\r\n`, the server can block forever waiting for more data while the client waits for a response.
- Under load, stuck connections leak file descriptors and can trigger `EMFILE` ("Too many open files").

**Concepts:** HTTP message framing, incremental reads, delimiter-based parsing.

---

## 5. Concepts Demonstrated

| Category            | Concepts                                                                          |
| ------------------- | --------------------------------------------------------------------------------- |
| **Networking**      | TCP sockets, `bind`/`listen`/`accept`, `SO_REUSEADDR`, client-server model        |
| **HTTP**            | Request line, headers, `\r\n\r\n` framing, status codes, `Content-Type`, keep-alive |
| **C++ OOP**         | Class separation, header/source split, static methods                             |
| **RAII**            | `Socket` auto-closes on destruction                                               |
| **Move semantics**  | Move-only `Socket`, `packaged_task`, initializer lists                            |
| **Concurrency**     | Thread pool, detached vs pooled threads, shared resources                         |
| **Synchronization** | `mutex`, `condition_variable`, `lock_guard`, `unique_lock`                        |
| **Observability**   | Level-based logging, access logs, latency measurement                             |
| **Build system**    | CMake, manual `clang++` compilation, linker errors (undefined symbols)            |
| **Filesystem**      | `std::filesystem`, static file serving                                            |

---

## 6. Build & Run

### Using CMake

```bash
mkdir build && cd build
cmake ..
make
./http_server
```

### Manual compile

```bash
clang++ -std=c++20 -Wall -Wextra -Iinclude \
  src/main.cpp src/HTTPServer.cpp src/HTTPParser.cpp \
  src/HTTPResponse.cpp src/FileServer.cpp src/Socket.cpp \
  src/MimeResolver.cpp src/Logger.cpp src/ThreadPool.cpp \
  -o http_server
```

### Run

```bash
./http_server              # default: port 8001, root public/
./http_server 8080 www     # custom port and document root
```

### Test

```bash
# Single request
curl http://localhost:8001/

# Load test (parallel requests)
chmod +x test/load_test.sh
./test/load_test.sh
```

---

## 7. Example Interactions

**Browser:** `http://localhost:8001/`  
→ Serves `public/index.html`

**curl:**

```bash
curl -v http://localhost:8001/file.jpeg
```

**Server log:**

```
[2026-07-07 23:26:00] [INFO] Listening on port 8001
[2026-07-07 23:26:05] [ACCESS] GET /file.jpeg->200	 | 26363 B  | 2
```

---

## 8. Known Limitations

| Limitation              | Details                                                                           |
| ----------------------- | --------------------------------------------------------------------------------- |
| **GET only**            | No POST, PUT, DELETE                                                              |
| **Partial body reads**  | Headers buffered until `\r\n\r\n`; request body (POST) not yet handled            |
| **No body parsing**     | POST/PUT not supported                                                            |
| **Path traversal risk** | `/../` not sanitized                                                              |
| **Some raw stdout**     | A few debug prints in `handleClient` still use `std::cout` instead of `Logger`    |
| **No HTTPS**            | Plain HTTP only                                                                   |
| **Keep-alive partial**  | Loop exists but no explicit `Connection` header handling                          |
| **Single directory**    | All files under `public/`                                                         |

---

## 9. Evolution Timeline

1. **Monolith** — single `.cpp` with all classes inline
2. **Modular split** — headers in `include/`, sources in `src/`
3. **MIME types** — `MimeResolver` for correct `Content-Type`
4. **Logging** — `Logger` with timestamps and latency
5. **Concurrency** — `std::thread` → **ThreadPool** with `packaged_task`
6. **Thread safety** — mutex on logger after observing stdout races under load
7. **Structured logging** — `Logger` expanded with `info` / `debug` / `warning` / `error` / `access`; used in `Socket`, `ThreadPool`, `FileServer`, and `HTTPServer`
8. **`SO_REUSEADDR`** — `Socket::setReuseAddress()` called before `bind()` for faster restarts
9. **Request framing** — buffer until `\r\n\r\n` before parsing; fixes deadlock and fd leaks under concurrent load

---

## 10. Future Improvements

- [x] Add `SO_REUSEADDR` in `Socket::bind()`
- [x] Request buffering until `\r\n\r\n` (robust header parsing)
- [x] Structured `Logger` with levels (`info`, `debug`, `warning`, `error`, `access`)
- [x] Configurable port and document root
- [ ] Route remaining `std::cout` debug output through `Logger`
- [ ] Skip invalid sockets after failed `accept()` (`!client.isValid()`)
- [ ] Configurable log level (e.g. `LOG_LEVEL=debug`)
- [ ] Path normalization / traversal protection
- [ ] Explicit `Connection: close` or full keep-alive support
- [ ] `HTTP/1.1` chunked encoding for large files
- [ ] Graceful shutdown on SIGINT
- [ ] Unit tests for parser and response serialization

---

## 11. Dependencies

- **C++17** (CMake) or **C++20** (manual build for `ends_with`)
- POSIX sockets (`sys/socket.h`, `netinet/in.h`, `unistd.h`)
- Standard library: `<thread>`, `<mutex>`, `<condition_variable>`, `<filesystem>`, `<future>`

**Platform:** macOS / Linux (POSIX). Not Windows-native without Winsock changes.

---

## 12. Quick Reference — Who Calls Whom

```
main()
  └── HTTPServer::start()
        ├── Socket::create/bind/listen
        ├── Socket::accept()          [loop]
        └── ThreadPool::submit()
              └── HTTPServer::handleClient()
                    ├── Socket::receive()       [until \r\n\r\n]
                    ├── HTTPParser::parse()
                    ├── FileServer::exists/readFile()
                    ├── MimeResolver::getMime()
                    ├── HTTPResponse::serialize()
                    ├── Socket::send()
                    └── Logger::access()
```

---

*This document describes the http-server project as of July 2026. It is intended as both a user guide and a learning reference for systems programming in C++.*
