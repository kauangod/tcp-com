#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mqueue.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#define PORT 7777
#define MAX_QUEUED_CONNECTIONS 5
#define BUFF_SIZ 256
std::vector<int> clients_list;
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  int sock;
  char queue_name[32];
  struct mq_attr attr;
} thread_args;

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
void *recv_client_thread(void *arg) {
  std::string request;
  int recv_status, sock;
  sock = ((thread_args *)arg)->sock;

  mqd_t mq = mq_open(((thread_args *)arg)->queue_name, O_CREAT | O_WRONLY, 0666,
                     &(((thread_args *)arg)->attr));
  for (;;) {
    int return_send;
    request.resize(BUFF_SIZ);
    recv_status = recv(sock, request.data(), request.size(), 0);
    if (recv_status <= 0) {
      if (recv_status < 0) {
        perror("recv");
      }
      continue;
    }
    request[recv_status] = '\0';
    std::cout << request << std::endl;
    return_send =
        mq_send(mq, request.data(), static_cast<size_t>(recv_status), 0);
    if (return_send == -1) {
      perror("mq_send");
    }
    if (!strcmp(request.data(), "Sair")) {
      request.clear();
      mq_close(mq);
      mq_unlink(((thread_args *)arg)->queue_name);
      pthread_exit(nullptr);
    }
    request.clear();
  }
  return nullptr;
}

void *send_client_thread(void *arg) {
  sleep(1);
  std::string request;
  std::string msg;
  ssize_t msg_size = 0;
  int sock, i = 0;
  pthread_t id = pthread_self();
  sock = ((thread_args *)arg)->sock;
  int file_err = 0;
  mqd_t mq = mq_open(((thread_args *)arg)->queue_name, O_RDONLY, 0666,
                     &(((thread_args *)arg)->attr));
  request.resize(BUFF_SIZ);
  for (;;) {
    request.resize(BUFF_SIZ);
    msg_size = mq_receive(mq, request.data(), request.size(), NULL);
    if (msg_size >= 0) {
    } else {
      perror("mq_receive");
      continue;
    }
    if (strstr(request.data(), "Chat") == NULL) {
      pthread_mutex_lock(&print_lock);
      std::cout << request << std::endl;
      pthread_mutex_unlock(&print_lock);
    }
    if (!strcmp(request.data(), "Sair")) {
      msg = "Tchau cliente " + std::to_string(id) + '\0';
      pthread_mutex_lock(&print_lock);
      printf("%s", msg.data());
      pthread_mutex_unlock(&print_lock);
      send(sock, msg.data(), msg.size(), 0);
      pthread_mutex_lock(&clients_mtx);
      clients_list.erase(
          std::remove(clients_list.begin(), clients_list.end(), sock),
          clients_list.end());
      pthread_mutex_unlock(&clients_mtx);
      request.clear();
      mq_close(mq);
      mq_unlink(((thread_args *)arg)->queue_name);
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
          msg = "ERRForneça o nome do arquivo requisitado";
          send(sock, msg.c_str(), msg.length(), 0);
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
        msg = "ERRArquivo não encontrado!";
        send(sock, msg.c_str(), msg.length(), 0);
        continue;
      }
      request.clear();
      std::streamsize bytes_read = 0, size = ifs.tellg();
      ifs.seekg(0, std::ios::beg);
      pthread_mutex_lock(&clients_mtx);
      send(sock, &size, sizeof(size), 0);
      pthread_mutex_unlock(&clients_mtx);
      char buf[BUFF_SIZ]; // qlqr coisa volta pro heap aqui
      char buff[4096];
      while (!ifs.eof()) {
        // std::cout << "Ta rodando o while do arquivo!" << std::endl;
        ifs.read(buff, 4096);
        bytes_read = ifs.gcount();
        if (bytes_read <= 0)
          break;
        std::cout << bytes_read << std::endl;
        send(sock, buff, bytes_read, 0);
        // sleep(2);
      }
      std::string local_hash = sha256_file(file_name);
      send(sock, local_hash.data(), local_hash.size(), 0);
      ifs.close();
    }
    request.clear();
  }
  return nullptr;
}

void *console_thread(void *arg) {
  while (true) {
    std::string msg;
    std::cout << "Mensagem do servidor para o cliente:";
    std::getline(std::cin, msg);
    std::cout << std::endl;
    msg += '\n';

    pthread_mutex_lock(&clients_mtx);

    for (int fd : clients_list) {
      send(fd, msg.c_str(), msg.size(), 0);
    }

    pthread_mutex_unlock(&clients_mtx);
  }

  return nullptr;
}

int main() {
  int serverfd, bind_status, thread_status, cons_thread_status, id = 0;
  socklen_t addr_len;
  struct sockaddr_in addr;
  pthread_t sender_thread, receiver_thread, cons_thread;
  cons_thread_status = pthread_create(&cons_thread, NULL, console_thread, NULL);
  if (cons_thread_status != 0)
    std::cout << "Erro na criação da thread do console" << std::endl;
  serverfd = socket(AF_INET, SOCK_STREAM, 0);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = INADDR_ANY;
  addr_len = sizeof(addr);
  bind_status = bind(serverfd, (struct sockaddr *)&addr, addr_len);
  listen(serverfd, MAX_QUEUED_CONNECTIONS);

  thread_args *args = new thread_args();

  for (;;) {
    int client_sock = accept(serverfd, (struct sockaddr *)&addr, &addr_len);
    if (client_sock < 0) {
      std::cerr << "Erro na criação do socket para o cliente!" << std::endl;
      break;
    }
    thread_args *args = new thread_args();
    args->sock = client_sock;
    args->attr.mq_flags = 0;
    args->attr.mq_maxmsg = 10;
    args->attr.mq_msgsize = BUFF_SIZ;
    args->attr.mq_curmsgs = 0;
    snprintf(args->queue_name, sizeof(args->queue_name), "/cliente_%d", id++);
    mq_unlink(args->queue_name);

    pthread_mutex_lock(&clients_mtx);
    clients_list.push_back(args->sock);
    pthread_mutex_unlock(&clients_mtx);

    thread_status =
        pthread_create(&sender_thread, NULL, send_client_thread, (void *)args);

    if (thread_status != 0) {
      std::cerr << "Erro na criação da thread de envio do cliente!"
                << std::endl;
      break;
    }

    thread_status = pthread_create(&receiver_thread, NULL, recv_client_thread,
                                   (void *)args);

    if (thread_status != 0) {
      std::cerr << "Erro na criação da thread de envio do cliente!"
                << std::endl;
      break;
    }
  }
  return 0;
}
