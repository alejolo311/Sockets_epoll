#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

// Quick and dirty assertion that a call succeeds
int Check(int rc, const char* call) {
  if (rc < 0) {
    perror(call);
    abort();
  }
  printf("%s => %d\n", call, rc);

  return rc;
}

#define CHECK(X) Check(X, #X)

int main(int argc, char* argv[]) {
  if (!argv[1]) {
    printf("usage: %s <port>\n", argv[0]);
    return 1;
  }

  signal(SIGPIPE, SIG_IGN);

  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(atoi(argv[1]));
  sa.sin_addr.s_addr = htonl(INADDR_ANY);

  int server = CHECK(socket(AF_INET, SOCK_STREAM, 0));
  CHECK(bind(server, (struct sockaddr*) &sa, sizeof(sa)));
  CHECK(listen(server, 512));
  int optval = 1;
  CHECK(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)));

  int flags;
  CHECK(fcntl(server, F_GETFL, &flags));
  CHECK(fcntl(server, F_SETFL, flags | O_NONBLOCK));

  struct epoll_event ev;
# define EVENTSZ 10
  struct epoll_event events[EVENTSZ];
  int epollfd = CHECK(epoll_create1(0));

  ev.events = EPOLLIN;
  ev.data.fd = server;
  CHECK(epoll_ctl(epollfd, EPOLL_CTL_ADD, server, &ev)); // why fd twice?

  // timeout of -1 means "wait for ever"
  printf("polling...\n");
  for(;;) {
    int nfds = CHECK(epoll_wait(epollfd, events, EVENTSZ, -1));

    for (int n = 0; n < nfds; n++) {
      if(events[n].data.fd == server) {
        printf("accepting new connection...\n");
        int client = CHECK(accept(server, NULL, 0));
        ev.events = EPOLLIN;
        ev.data.fd = client;
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_ADD, client, &ev));
      } else {
        printf("client %d, reading...\n", events[n].data.fd);
        char buf[4096];
        ssize_t buflen = read(events[n].data.fd, buf, sizeof(buf));
        if (buflen > 0) {
          printf("  writing %zd bytes...\n", buflen);
          buflen = write(events[n].data.fd, buf, buflen);
        }
        
        if (buflen < 1) {
          printf("  clearing client!\n");
          close(events[n].data.fd);
        }
      }
    }
  }

  return 0;
}