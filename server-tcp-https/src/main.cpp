#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <fstream>
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
#define BUFF_SIZ 4096
std::vector<int> clients_list;
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  int sock;
  char queue_name[32];
  struct mq_attr attr;
} thread_args;

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
    return_send =
        mq_send(mq, request.data(), static_cast<size_t>(recv_status), 0);
    if (return_send == -1) {
      perror("mq_send");
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
  for (;;) {
    request.resize(BUFF_SIZ);
    msg_size = mq_receive(mq, request.data(), request.size(), NULL);
    if (msg_size >= 0) {
    } else {
      perror("mq_receive");
      continue;
    }
    std::string file_name;
    size_t posGET = request.find("GET ");
    posGET += 4;
    size_t posSpace = request.find(' ', posGET);
    std::string path = request.substr(posGET, posSpace - posGET);
    if (!path.empty() && path[0] == '/')
      path.erase(0, 1);

    file_name = path;
    std::ifstream ifs(file_name, std::ios::binary | std::ios::ate);
    std::streamsize bytes_read = 0, size = 0;
    bool is_html = false;
    if (file_name.find("html") != std::string::npos)
      is_html = true;
    if (!ifs.is_open()) {
      std::string content = "<html><body><h1>404 - Not "
                            "Found</h1></body></html>";
      std::string header = "HTTP/1.0 404 Not Found\r\nContent-Type: "
                           "text/html\r\nContent-Length: " +
                           std::to_string(content.size()) + "\r\n\r\n";
      send(sock, header.data(), header.size(), 0);
      send(sock, content.data(), content.size(), 0);
      continue;
    }
    std::string header, content;
    size = ifs.tellg();
    content.resize(size);
    ifs.seekg(0, std::ios::beg);
    ifs.read(content.data(), size);
    if (is_html) {
      header = "HTTP/1.0 200 OK\r\nContent-Type: "
               "text/html\r\nContent-Length: " +
               std::to_string(size) + "\r\n\r\n";
    } else {
      header =
          "HTTP/1.0 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: " +
          std::to_string(size) + "\r\n\r\n";
    }
    send(sock, header.data(), header.size(), 0);
    send(sock, content.data(), size, 0);
    is_html = false;
    ifs.close();
    request.clear();
  }
  return nullptr;
}

int main() {
  int serverfd, bind_status, thread_status, id = 0;
  socklen_t addr_len;
  struct sockaddr_in addr;
  pthread_t sender_thread, receiver_thread;
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
