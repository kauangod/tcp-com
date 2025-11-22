#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#define PORT 7777
#define MAX_QUEUED_CONNECTIONS 5

std::vector<int> client_list;
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

void *client_thread(void *arg) {
  std::string request;
  std::string msg;
  int recv_status, sock, i = 0;
  pthread_t id = pthread_self();
  sock = *(int *)arg;
  int file_err = 0;

  // std::getline(std::cin >> std::ws, command);
  for (;;) {
    request.resize(100);
    recv_status = recv(sock, request.data(), request.size(), 0);
    request[recv_status] = '\0';
    std::cout << request << std::endl;
    if (!strcmp(request.data(), "Sair")) {
      msg = "Tchau cliente " + std::to_string(id) + '\0';
      send(sock, msg.data(), msg.size(), 0);
      close(sock);
      pthread_exit(nullptr);
    } else if (strstr(request.data(), "Chat") != NULL) {
      pthread_mutex_lock(&print_lock);
      std::cout << "Cliente " << id << ": " << request.data() << std::endl;
      pthread_mutex_unlock(&print_lock);
    } else {
      for (i = 0;; i++) {
        if (request[i] == '/' && (i + 1) < (request.size() - 1)) {
          i++;
          file_err = 0;
          break;
        } else if ((i + 1) > (request.size() - 1)) {
          std::cerr << "Forneça o nome do arquivo requisitado" << std::endl;
          file_err = -1;
          break;
        }
      }

      if (file_err == -1) {
        continue;
      }
      std::string file_name;

      for (; i < request.size(); i++) {
        file_name += request[i];
      }
      std::ifstream ifs(file_name, std::ios::binary | std::ios::ate);
      if (!ifs.is_open()) {
        msg = "Arquivo não encontrado!";
        send(sock, msg.c_str(), msg.length(), 0);
        continue;
      }
      request.clear();
      std::streamsize bytes_read = 0, size = ifs.tellg();
      ifs.seekg(0, std::ios::beg);
      send(sock, &size, sizeof(size), 0);
      recv(sock, request.data(), request.size(), 0);
      while (!ifs.eof()) {
        char *buff = new char[4096];
        ifs.read(buff, 4096);
        bytes_read = ifs.gcount();
        send(sock, buff, bytes_read, 0);
        delete[] buff;
      }
      ifs.close();
    }
    request.clear();
  }
  return nullptr;
}

int main() {
  int serverfd, bind_status, thread_status;
  int *client_sck = new int();
  socklen_t addr_len;
  struct sockaddr_in addr;
  pthread_t thread_id;

  serverfd = socket(AF_INET, SOCK_STREAM, 0);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = INADDR_ANY;
  addr_len = sizeof(addr);
  bind_status = bind(serverfd, (struct sockaddr *)&addr, addr_len);
  listen(serverfd, MAX_QUEUED_CONNECTIONS);

  for (;;) {
    *client_sck = accept(serverfd, (struct sockaddr *)&addr, &addr_len);
    if (*client_sck < 0) {
      std::cerr << "Erro na criação do socket para o cliente!" << std::endl;
      break;
    }

    client_list.push_back(*client_sck);
    thread_status =
        pthread_create(&thread_id, NULL, client_thread, (void *)client_sck);

    if (thread_status != 0) {
      std::cerr << "Erro na criação da thread do cliente!" << std::endl;
      break;
    }
    client_sck = new int();
  }

  return 0;
}
