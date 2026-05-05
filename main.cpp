#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <ctime>
#include <map>
#include <sstream>

// windows is special and needs its own socket garbage, classic microsoft moment
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket  // windows calls it closesocket because why not
#else
    // normal people (linux) use these
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    // make linux look like windows so the rest of the code doesnt have a meltdown
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CLOSE_SOCKET close
#endif

// reads crestline.conf line by line and builds a key=value map
// if you cant figure out the config format you should probably give up
std::map<std::string, std::string> loadConfig(const std::string& filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // skip comments and empty lines
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            config[key] = value;
        }
    }
    return config;
}

// figures out what type of file we're serving so the browser doesnt lose its mind
std::string getMimeType(const std::string& path) {
    std::string ext = path.substr(path.find_last_of('.'));
    if (ext == ".html") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".json") return "application/json";
    return "text/plain"; // no idea what this is, good luck browser
}

// logs every request to terminal and server.log
// yes we log 404s too, especially to shame whoever typed the wrong url
void logRequest(const std::string& path, int statusCode) {
    time_t now = time(0);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", localtime(&now));

    std::string logEntry = "[" + std::string(timestamp) + "] GET " + path + " " + std::to_string(statusCode);

    std::cout << logEntry << "\n";

    std::ofstream logFile("server.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << logEntry << "\n";
        logFile.close();
    }
}

// reads the full http request properly instead of just hoping it fits in 4096 bytes
// http requests end with \r\n\r\n so we keep reading until we find that
std::string readFullRequest(SOCKET clientSocket) {
    std::string request;
    char chunk[1024];

    while (true) {
        int bytesRead = recv(clientSocket, chunk, sizeof(chunk), 0);
        // set timeout so recv doesnt block forever
#ifdef _WIN32
        DWORD timeout = 5000; // 5 seconds
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
#else
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
        if (bytesRead <= 0) break; // connection closed or error, bail out
        request.append(chunk, bytesRead);
        if (request.find("\r\n\r\n") != std::string::npos) break; // got the full headers, done
    }

    return request;
}

// handles one client connection - reads request, sends response, closes socket
// runs in its own thread so multiple clients dont block each other
void handleClient(SOCKET clientSocket, std::string publicDir) {
    // read the full request instead of praying it fits in a fixed buffer
    std::string request = readFullRequest(clientSocket);

    // parse the path out of "GET /index.html HTTP/1.1"
    std::string path = request.substr(request.find(' ') + 1);
    path = path.substr(0, path.find(' '));

    std::string response;

    // api endpoints - returns json instead of a file
    if (path == "/api/status") {
        std::string json = "{\"status\": \"ok\", \"server\": \"Crestline\"}";
        response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + json;
        logRequest(path, 200);
    } else {
        // serve files from the public directory
        if (path == "/") path = "/index.html"; // default to index.html because obviously

        std::string filePath = publicDir + path;
        std::ifstream file(filePath);

        if (file.is_open()) {
            // file exists, send it
            std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
            std::string mime = getMimeType(path);
            response = "HTTP/1.1 200 OK\r\nContent-Type: " + mime + "\r\n\r\n" + content;
            logRequest(path, 200);
        } else {
            // file doesnt exist, serve 404 page
            // if 404.html is also missing thats your problem not mine
            std::ifstream errorFile(publicDir + "/404.html");
            if (errorFile.is_open()) {
                std::string content((std::istreambuf_iterator<char>(errorFile)),
                            std::istreambuf_iterator<char>());
                response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n" + content;
            } else {
                response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
            }
            logRequest(path, 404);
        }
    }

    send(clientSocket, response.c_str(), response.size(), 0);
    CLOSE_SOCKET(clientSocket);
}

int main() {
    // load config - if this crashes you forgot to create crestline.conf
    auto config = loadConfig("crestline.conf");
    int port = std::stoi(config["port"]);
    std::string publicDir = config["public_dir"];

    // windows needs this garbage to use sockets, linux doesnt because linux is better
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "Socket creation failed!\n";
        return 1;
    }
    std::cout << "Socket created!\n";

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Socket binding failed!\n";
        return 1;
    }
    std::cout << "Socket bound on 0.0.0.0:" << port << "\n";

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "Socket listening failed!\n";
        return 1;
    }
    std::cout << "Crestline running on http://localhost:" << port << "\n";

    // main loop - accept connections and handle them in separate threads
    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        std::thread(handleClient, clientSocket, publicDir).detach();
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}