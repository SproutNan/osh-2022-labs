#define _1_cpp

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <vector>

#include "0.h"

void *handle_chat(void *data)
{
    struct Pipe *pipe = (struct Pipe *)data; // 在线程中解引用
    while (1)
    {
        char buffer[MAX_SINGLE_MESSAGE_LENGTH] = "";
        ssize_t len = recv(pipe->fd_send, buffer, MAX_SINGLE_MESSAGE_LENGTH, 0);
        // send(pipe->fd_recv, std::to_string(len).data(), std::to_string(len).length(), 0);

        if (len <= 0)
            break;

        auto mess_box = split(buffer, "\n");
        auto messes = mess_box.first;
        auto clip_num = mess_box.second;
        for (int i = 0; i < clip_num; i++)
        {
            if (messes[i].length())
            {
                std::string mess = " [Message]";
                mess += messes[i];
                mess += "\n";
                int send_len = 0;
                do
                {
                    int send_num = send(pipe->fd_recv, mess.data(), mess.length(), 0); // 本次发送的字符数
                    mess = mess.substr(send_num);                                      // 假如本次发了4个字符，就要从原来字符串的第4个字符重新开始发
                    send_len = mess.length();                                          // 需要发送的字符数
                } while (send_len);
            }
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int port = atoi(argv[1]);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind");
        return 1;
    }
    if (listen(fd, 2))
    {
        perror("listen");
        return 1;
    }
    int fd1 = accept(fd, NULL, NULL);
    int fd2 = accept(fd, NULL, NULL);
    if (fd1 == -1 || fd2 == -1)
    {
        perror("accept");
        return 1;
    }
    pthread_t thread1, thread2;
    struct Pipe pipe1;
    struct Pipe pipe2;
    pipe1.fd_send = fd1;
    pipe1.fd_recv = fd2;
    pipe2.fd_send = fd2;
    pipe2.fd_recv = fd1;
    pthread_create(&thread1, NULL, handle_chat, (void *)&pipe1);
    pthread_create(&thread2, NULL, handle_chat, (void *)&pipe2);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    return 0;
}
