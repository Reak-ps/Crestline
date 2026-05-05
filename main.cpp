#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <ctime>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CLOSE_SOCKET close
#endif

// ─── Config ──────────────────────────────────────────────────────────────────

std::map<std::string, std::string> loadConfig(const std::string& filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            config[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }
    return config;
}

int getConfigInt(const std::map<std::string, std::string>& config,
                 const std::string& key, int fallback) {
    auto it = config.find(key);
    if (it == config.end() || it->second.empty()) return fallback;
    try { return std::stoi(it->second); }
    catch (...) { return fallback; }
}

std::string getConfigStr(const std::map<std::string, std::string>& config,
                         const std::string& key, const std::string& fallback) {
    auto it = config.find(key);
    return (it != config.end() && !it->second.empty()) ? it->second : fallback;
}

// ─── MIME types ───────────────────────────────────────────────────────────────

std::string getMimeType(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return "application/octet-stream";

    std::string ext = path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css")                   return "text/css";
    if (ext == ".js")                    return "application/javascript";
    if (ext == ".json")                  return "application/json";
    if (ext == ".png")                   return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")                   return "image/gif";
    if (ext == ".svg")                   return "image/svg+xml";
    if (ext == ".ico")                   return "image/x-icon";
    if (ext == ".woff2")                 return "font/woff2";
    if (ext == ".woff")                  return "font/woff";
    return "application/octet-stream";
}

// ─── Logging ─────────────────────────────────────────────────────────────────

void logRequest(const std::string& method, const std::string& path, int statusCode) {
    time_t now = time(nullptr);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", localtime(&now));

    std::string entry = "[" + std::string(timestamp) + "] "
                      + method + " " + path + " " + std::to_string(statusCode);

    std::cout << entry << "\n";

    std::ofstream logFile("server.log", std::ios::app);
    if (logFile.is_open()) logFile << entry << "\n";
}

// ─── Path safety ──────────────────────────────────────────────────────────────

// Resolves ".." segments without touching the filesystem.
// Returns empty string if the resolved path escapes the root.
std::string sanitizePath(const std::string& rawPath) {
    std::istringstream ss(rawPath);
    std::string segment;
    std::vector<std::string> parts;

    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".") continue;
        if (segment == "..") {
            if (!parts.empty()) parts.pop_back();
            // attempted escape — reject
            else return "";
        } else {
            parts.push_back(segment);
        }
    }

    std::string clean = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        clean += parts[i];
        if (i + 1 < parts.size()) clean += "/";
    }
    return clean;
}

// ─── Request reading ─────────────────────────────────────────────────────────

std::string readFullRequest(SOCKET clientSocket) {
    // Set receive timeout before the first read
#ifdef _WIN32
    DWORD timeout = 5000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
#else
    struct timeval timeout{5, 0};
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

    std::string request;
    char chunk[1024];

    while (true) {
        int bytesRead = recv(clientSocket, chunk, sizeof(chunk), 0);
        if (bytesRead <= 0) break;
        request.append(chunk, bytesRead);
        if (request.find("\r\n\r\n") != std::string::npos) break;
    }
    return request;
}

// ─── Response helpers ────────────────────────────────────────────────────────

std::string makeResponse(int code, const std::string& status,
                         const std::string& mime, const std::string& body) {
    return "HTTP/1.1 " + std::to_string(code) + " " + status + "\r\n"
         + "Content-Type: " + mime + "\r\n"
         + "Content-Length: " + std::to_string(body.size()) + "\r\n"
         + "Connection: close\r\n"
         + "\r\n"
         + body;
}

// Reads a file in binary mode into a string. Returns false on failure.
bool readFileBinary(const std::string& filePath, std::string& out) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(file),
               std::istreambuf_iterator<char>());
    return true;
}

// ─── Request parsing ─────────────────────────────────────────────────────────

struct HttpRequest {
    std::string method;
    std::string path;
    bool valid = false;
};

HttpRequest parseRequest(const std::string& raw) {
    HttpRequest req;
    if (raw.empty()) return req;

    size_t methodEnd = raw.find(' ');
    if (methodEnd == std::string::npos) return req;

    size_t pathEnd = raw.find(' ', methodEnd + 1);
    if (pathEnd == std::string::npos) return req;

    req.method = raw.substr(0, methodEnd);
    req.path   = raw.substr(methodEnd + 1, pathEnd - methodEnd - 1);
    req.valid  = true;
    return req;
}

// ─── Client handler ───────────────────────────────────────────────────────────

void handleClient(SOCKET clientSocket, const std::string& publicDir) {
    std::string raw = readFullRequest(clientSocket);
    HttpRequest req = parseRequest(raw);

    std::string response;

    if (!req.valid) {
        response = makeResponse(400, "Bad Request", "text/plain", "Bad Request");
        send(clientSocket, response.c_str(), (int)response.size(), 0);
        CLOSE_SOCKET(clientSocket);
        return;
    }

    if (req.path == "/api/status") {
        std::string json = R"({"status":"ok","server":"Crestline","version":"1.0"})";
        response = makeResponse(200, "OK", "application/json", json);
        logRequest(req.method, req.path, 200);

    } else {
        std::string safePath = sanitizePath(req.path);

        if (safePath.empty()) {
            response = makeResponse(400, "Bad Request", "text/plain", "Bad Request");
            logRequest(req.method, req.path, 400);

        } else {
            if (safePath == "/") safePath = "/index.html";

            std::string filePath = publicDir + safePath;
            std::string body;

            if (readFileBinary(filePath, body)) {
                response = makeResponse(200, "OK", getMimeType(safePath), body);
                logRequest(req.method, req.path, 200);
            } else {
                // Try a custom 404 page, fall back to inline
                if (!readFileBinary(publicDir + "/404.html", body)) {
                    body = "<h1>404 Not Found</h1>";
                }
                response = makeResponse(404, "Not Found", "text/html; charset=utf-8", body);
                logRequest(req.method, req.path, 404);
            }
        }
    }

    send(clientSocket, response.c_str(), (int)response.size(), 0);
    CLOSE_SOCKET(clientSocket);
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::map<std::string, std::string> config;
    try {
        config = loadConfig("crestline.conf");
    } catch (const std::exception& e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    }

    int port                 = getConfigInt(config, "port", 8080);
    std::string publicDir    = getConfigStr(config, "public_dir", "public");

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    // Allow immediate reuse of the port after restart
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    std::cout << "Socket created\n";

    sockaddr_in serverAddr{};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed — is port " << port << " already in use?\n";
        CLOSE_SOCKET(serverSocket);
        return 1;
    }
    std::cout << "Socket bound on 0.0.0.0:" << port << "\n";

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        CLOSE_SOCKET(serverSocket);
        return 1;
    }
    std::cout << "Crestline running on http://localhost:" << port << "\n";

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) continue; // don't spawn a thread on a bad socket
        std::thread(handleClient, clientSocket, publicDir).detach();
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}