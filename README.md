# Crestline 🌊

A lightweight, cross-platform HTTP server written in C++ from scratch. No frameworks, no bloat — just raw sockets.

## Features

- ⚡ Multi-threaded — handles multiple connections simultaneously
- 📁 Static file serving (HTML, CSS, JS, PNG, JPG)
- 🔌 JSON API endpoints out of the box (`/api/status`)
- 📋 Request logging — terminal + `server.log`
- ⚙️ Simple config file — everything in one place
- 🐧 Cross-platform — Windows & Linux

## Getting Started

### Requirements
- C++20 compiler (GCC / MSVC)
- CMake 3.x+
- CLion or any C++ IDE

### Build

```bash
git clone https://github.com/Reak-ps/Crestline
cd Crestline
mkdir build && cd build
cmake ..
make
```

### Run

```bash
./Crestline
```

Then open your browser at `http://localhost:8080`

## Configuration

Edit `crestline.conf` before starting the server:

```ini
# port to listen on
port=8080

# folder to serve files from
public_dir=public

# default page
index_file=index.html

# enable request logging
logging=true

# log file location
log_file=server.log
```

## File Structure

```
Crestline/
├── public/         # put your HTML, CSS, JS here
│   ├── index.html
│   ├── style.css
│   └── 404.html
├── main.cpp
├── crestline.conf
└── CMakeLists.txt
```

## API

| Endpoint | Method | Response |
|----------|--------|----------|
| `/api/status` | GET | `{"status": "ok", "server": "Crestline"}` |

## Benchmark

> Tested on 

```
autocannon -c 900 -d 30 http://localhost:8080

```

## Built with

- C++20
- Raw Winsock2 / POSIX sockets
- Zero external dependencies

## See my other projects

👉 [My Repositories](https://github.com/Reak-ps?tab=repositories)
EOF
