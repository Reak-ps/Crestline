#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

void handleClient(SOCKET clientSocket) {
	char buffer[4096] = {0};
	recv(clientSocket, buffer, sizeof(buffer), 0);

	std::string request(buffer);
	std::string path = request.substr(request.find(' ') + 1);
	path = path.substr(0, path.find(' '));

	if (path == "/") path = "/index.html";

	std::string filePath = "public" + path;
	std::ifstream file(filePath);

	std::string response;
	if (file.is_open()) {
		std::string content((std::istreambuf_iterator<char>(file)),
							 std::istreambuf_iterator<char>());
		response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + content;
	} else {
		response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
	}

	send(clientSocket, response.c_str(), response.size(), 0);
	closesocket(clientSocket);
}
int main() {
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
	serverAddr.sin_port = htons(8080);

	if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cout << "Socket binding failed!\n";
		return 1;
	}
	std::cout << "Socket bound on 0.0.0.0:8080!\n";

	if (listen(serverSocket, 10) == SOCKET_ERROR) {
		std::cout << "Socket listening failed!\n";
		return 1;
	}
	std::cout << "Socket listening on port 8080\n";

	while (true) {
		SOCKET clientSocket = accept(serverSocket, NULL, NULL);
		std::thread(handleClient, clientSocket).detach();
	}


	WSACleanup();
	return 0;
}