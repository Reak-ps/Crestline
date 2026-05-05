#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <fstream>
#include <ctime>
#include <map>
#include <sstream>

std::map<std::string, std::string> loadConfig(const std::string& filename) {
	std::map<std::string, std::string> config;
	std::ifstream file(filename);
	std::string line;

	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#') continue;  // Kommentare überspringen
		size_t pos = line.find('=');
		if (pos != std::string::npos) {
			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + 1);
			config[key] = value;
		}
	}
	return config;
}

#pragma comment(lib, "ws2_32.lib")
std::string getMimeType(const std::string& path) {
	  std::string ext = path.substr(path.find_last_of('.'));
		if (ext == ".html") return "text/html";
	    if (ext == ".css") return "text/css";
	    if (ext == ".js") return "application/javascript";
	    if (ext == ".png") return "image/png";
	    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
	    return "text/plain";
}
void logRequest(const std::string& path, int statusCode) {  // Fix 1: richtige Parameter
	time_t now = time(0);
	char timestamp[20];
	strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", localtime(&now));

	std::string logEntry = "[" + std::string(timestamp) + "] GET " + path + " " + std::to_string(statusCode);  // Fix 2: timestamp nicht timestap

	std::cout << logEntry << "\n";

	std::ofstream logFile("server.log", std::ios::app);
	if (logFile.is_open()) {
		logFile << logEntry << "\n";
		logFile.close();
	}
}

void handleClient(SOCKET clientSocket, std::string publicDir) {
	char buffer[4096] = {0};
	recv(clientSocket, buffer, sizeof(buffer), 0);

	std::string request(buffer);
	std::string path = request.substr(request.find(' ') + 1);
	path = path.substr(0, path.find(' '));

	std::string response;

	if (path == "/api/status") {
		std::string json = "{\"status\": \"ok\", \"server\": \"Crestline\"}";
		response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + json;
		logRequest(path, 200);
	} else {
		if (path == "/") path = "/index.html";
		std::string filePath = publicDir + path;
		std::ifstream file(filePath);

		if (file.is_open()) {
			std::string content((std::istreambuf_iterator<char>(file)),
							std::istreambuf_iterator<char>());
			std::string mime = getMimeType(path);
			response = "HTTP/1.1 200 OK\r\nContent-Type: " + mime + "\r\n\r\n" + content;
			logRequest(path, 200);
		} else {
			std::ifstream errorFile(publicDir + "/404.html");
			if (errorFile.is_open()) {
				std::string content((std::istreambuf_iterator<char>(errorFile)),
							std::istreambuf_iterator<char>());
				response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n" + content;
			}
			logRequest(path, 404);
		}
	}

	send(clientSocket, response.c_str(), response.size(), 0);
	closesocket(clientSocket);
}

int main() {
	auto config = loadConfig("crestline.conf");
	int port = std::stoi(config["port"]);
	std::string publicDir = config["public_dir"];


	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

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

	if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cout << "Socket binding failed!\n";
		return 1;
	}
	std::cout << "Socket bound on 0.0.0.0:" << port << "!\n";

	if (listen(serverSocket, 10) == SOCKET_ERROR) {
		std::cout << "Socket listening failed!\n";
		return 1;
	}
	std::cout << "Socket listening on port " << port << "!\n";

	while (true) {
		SOCKET clientSocket = accept(serverSocket, NULL, NULL);
		std::thread(handleClient, clientSocket, publicDir).detach();
	}


	WSACleanup();
	return 0;
}