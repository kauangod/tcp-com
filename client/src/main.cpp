#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <netinet/in.h>
#include <openssl/evp.h>
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

std::string sha256_file(const std::string &path) { /* made by AI */
  std::ifstream f(path, std::ios::binary);

  if (!f)
    throw std::runtime_error("erro ao abrir o arquivo");

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();

  if (!ctx)
    throw std::runtime_error("erro ao criar o contexto");

  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1)
    throw std::runtime_error("erro no digestInit");

  char buf[4096];

  while (f.good()) {
    f.read(buf, sizeof(buf));
    std::streamsize s = f.gcount();
    if (s > 0) {
      if (EVP_DigestUpdate(ctx, buf, s) != 1)
        throw std::runtime_error("erro no digestUpdate");
    }
  }
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len = 0;

  if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1)
    throw std::runtime_error("erro no digestFinal");

  EVP_MD_CTX_free(ctx);

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');

  for (unsigned i = 0; i < hash_len; ++i)
    oss << std::setw(2) << (int)hash[i];

  return oss.str();
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
  std::ifstream existing_file_check(file_name, std::ios::binary);

  if (existing_file_check.good()) {
    existing_file_check.close();
    std::remove(file_name.c_str());
  }
  for (;;) {
    char *buff = new char[MAX_BUFFER_SIZE];
    size_t b_recv = 0, first_recv_b = 0;
    bool flag_chat = false;
    send(clientfd, request.data(), request.size(), 0);
    if (strstr(request.data(), "Chat")) {
      flag_chat = true;
    } else if (!(request == "Sair") && strstr(request.data(), "Chat") == NULL) {
      first_recv_b = recv(clientfd, buff, MAX_BUFFER_SIZE, 0);
      if (strstr(buff, "Chat") != NULL) {
        flag_chat = true;
        buff[first_recv_b] = '\0';
        printf("%s", buff);
      } else {
        send(clientfd, "Manda", 5, 0);
      }
    }
    size_t temp_size = 0;
    for (;;) {
      if (flag_chat) {
        break;
      }
      b_recv = recv(clientfd, buff, MAX_BUFFER_SIZE, 0);
      std::cout << buff << std::endl;
      if (request == "Sair") {
        std::cout << buff << std::endl;
        delete[] buff;
        close(clientfd);
        return -1;
      } else if (buff[0] == 'E' && buff[1] == 'R' && buff[2] == 'R') {
        std::cout << &buff[3] << std::endl;
        break;
      } else {
        std::ofstream ofs(file_name, std::ios::binary | std::ios::app);
        if (!ofs.is_open()) {
          std::cerr << "Não foi possível abrir o arquivo!" << std::endl;
        }
        ofs.write(buff, b_recv);
        ofs.close();
        temp_size += b_recv;
        if (temp_size >= size) {
          std::string local_hash = sha256_file(file_name);
          std::string hash;
          hash.resize(64);
          send(clientfd, "Manda", 5, 0);
          recv(clientfd, hash.data(), hash.size(), 0);
          std::cout << "hash calculado no cliente: " << local_hash << std::endl;
          std::cout << "hash calculado na thread do server: " << hash
                    << std::endl;
          std::cout << "nome do arquivo: " << file_name << std::endl;
          std::cout << "tamanho do arquivo: " << std::fixed
                    << std::setprecision(2) << (float)temp_size / 1000000
                    << "MB" << std::endl;
          if (local_hash == hash) {
            std::cout << "Arquivo íntegro" << std::endl;
          } else {
            std::cerr << "Arquivo corrompido" << std::endl;
          }
          break;
        }
      }
      delete[] buff;
    }
    delete[] buff;
    std::cout << "Faça outra requisição ao servidor: " << std::endl;
    std::getline(std::cin >> std::ws, request);
  }

  return 0;
}
