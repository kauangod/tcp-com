#include <cstring>
#include <iostream>
#include <mqueue.h>
#include <unistd.h>

#define QNAME "/teste_mq"
#define MSIZE 128 // tamanho máximo da msg
#define MAXMSG 10 // quantidade de msgs

void sender() {
  mqd_t mq = mq_open(QNAME, O_WRONLY);
  if (mq == (mqd_t)-1) {
    perror("mq_open sender");
    return;
  }

  const char *msg = "GET @127.0.0.1:7777/text.txt";
  if (mq_send(mq, msg, strlen(msg) + 1, 0) == -1) {
    perror("mq_send");
  } else {
    std::cout << "Enviado: " << msg << std::endl;
  }
  mq_close(mq);
}

void receiver() {
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = MAXMSG;
  attr.mq_msgsize = MSIZE;
  attr.mq_curmsgs = 0;

  mqd_t mq = mq_open(QNAME, O_CREAT | O_RDONLY, 0666, &attr);
  if (mq == (mqd_t)-1) {
    perror("mq_open receiver");
    return;
  }

  char buffer[MSIZE];
  ssize_t n = mq_receive(mq, buffer, MSIZE, nullptr);

  if (n >= 0) {
    std::cout << "Recebido: " << buffer << std::endl;
  } else {
    perror("mq_receive");
  }

  mq_close(mq);
  mq_unlink(QNAME);
}

int main() {
  if (fork() == 0) {
    sleep(1); // dá tempo do receiver criar a fila
    sender();
  } else {
    receiver();
  }
  return 0;
}
