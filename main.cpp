#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM,0);
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
		if (clientSocket == INVALID_SOCKET) {
			std::cout << "Socket accept failed!\n";
			return 1;
		}
		std::cout << "Socket accepted!\n";
		char buffer[4096] = {0};
		recv(clientSocket, buffer, sizeof(buffer), 0);
		std::cout << "Request:\n" << buffer << "\n";
	}

	std::cout << "Winsock initialized!\n";

	WSACleanup();
	return 0;
}