#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 7777
#define MAX_QUEUED_CONNECTIONS 5
#define MAX_BUFFER_SIZE 4096

int parse_command(std::string command, int *o1, int *o2, int *o3, int *o4) {
  size_t p1 = command.find('.');
  size_t p2 = command.find('.', p1 + 1);
  size_t p3 = command.find('.', p2 + 1);
  int at_pos = command.find('@');

  if (at_pos == std::string::npos || p1 == std::string::npos ||
      p2 == std::string::npos || p3 == std::string::npos) {
    return 1;
  }

  *o1 = std::stoi(command.substr(at_pos + 1, p1 - (at_pos + 1)));
  *o2 = std::stoi(command.substr(p1 + 1, p2 - (p1 + 1)));
  *o3 = std::stoi(command.substr(p2 + 1, p3 - (p2 + 1)));
  *o4 = std::stoi(command.substr(p3 + 1));

  if (*o1 > 255 || *o2 > 255 || *o3 > 255 || *o4 > 255) {
    return -1;
  }
  return 0;
}

int main() {
  int clientfd, con_status, o1, o2, o3, o4, i, parse_result;
  socklen_t addr_len;
  struct sockaddr_in server_addr;
  std::string request, ip, file_name;

  std::cout << "Faça uma requisição ao servidor: ";
  std::getline(std::cin >> std::ws, request);
  parse_result = parse_command(request, &o1, &o2, &o3, &o4);
  if (parse_result == -1) {
    std::cerr << "Invalid IP address: octet exceeds 255." << std::endl;
    return -1;
  } else if (parse_result == 0) {
    ip = std::to_string(o1) + "." + std::to_string(o2) + "." +
         std::to_string(o3) + "." + std::to_string(o4);
  }
  clientfd = socket(AF_INET, SOCK_STREAM, 0);

  if (clientfd < 0) {
    std::cerr << "Erro na criação do socket para o cliente!" << std::endl;
  }

  bool is_a_file = true;

  for (i = 0; i < request.size(); i++) {
    if (request[i] == '/') {
      i++;
      break;
    } else if (i == request.size() - 1) {
      is_a_file = false;
      break;
    }
  }
  if (is_a_file) {
    for (; i < request.size(); i++) {
      file_name += request[i];
    }
  } else {
    ip = "127.0.0.1";
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  inet_pton(AF_INET, ip.data(), &server_addr.sin_addr);
  addr_len = sizeof(server_addr);
  con_status = connect(clientfd, (struct sockaddr *)&server_addr, addr_len);

  if (con_status < 0) {
    std::cerr << "Timeout" << std::endl;
  }

  std::streamsize size = 0;

  for (;;) {
    size_t b_recv = 0;
    send(clientfd, request.data(), request.size(), 0);
    if (!(request == "Sair") && strstr(request.data(), "Chat") == NULL) {
      printf("oi");
      recv(clientfd, &size, sizeof(size), 0);
      send(clientfd, "Manda", 5, 0);
    }
    size_t temp_size = 0;
    for (;;) {
      char *buff = new char[MAX_BUFFER_SIZE];
      b_recv = recv(clientfd, buff, MAX_BUFFER_SIZE, 0);
      if (request == "Sair") {
        std::cout << buff << std::endl;
        close(clientfd);
        return -1;
      } else if (strstr(buff, "Chat") != NULL) {
        std::cout << buff << std::endl;
        break;
      } else {
        std::ofstream ofs(file_name, std::ios::binary | std::ios::app);
        if (!ofs.is_open()) {
          std::cerr << "Não foi possível abrir o arquivo!" << std::endl;
        }
        ofs.write(buff, b_recv);
        ofs.close();
        temp_size += b_recv;
        std::cout << temp_size << std::endl;
        if (temp_size >= size) {
          break;
        }
      }
      delete[] buff;
    }
    std::cout << "Faça outra requisição ao servidor: " << std::endl;
    std::getline(std::cin >> std::ws, request);
  }

  return 0;
}
