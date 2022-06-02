#define _2_cpp

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <queue>

#include "0.h"

void *handle_chat(void *data);

std::queue<Message> message_queue;
Client clients[MAX_CLIENT_NUMBER];
int connected_clients_num = 0;

pthread_mutex_t Clients_Mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t Queue_Mutex = PTHREAD_MUTEX_INITIALIZER;

void client_destroy(Client *user)
{
    pthread_mutex_lock(&Clients_Mutex);
    user->connected = false;
    close(user->fd);
    connected_clients_num--;
    pthread_mutex_unlock(&Clients_Mutex);
    printf("[QUIT] one client left, now %d in total\n", connected_clients_num);
}

int client_add(int fd)
{
    int index = -1;
    pthread_mutex_lock(&Clients_Mutex);
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++)
    {
        if (!clients[i].connected)
        {
            index = i;
            break;
        }
    }
    if (index < 0 && index >= MAX_CLIENT_NUMBER)
    {
        pthread_mutex_unlock(&Clients_Mutex);
        return -1;
    }
    clients[index].connected = true;
    clients[index].fd = fd;
    connected_clients_num++;
    pthread_mutex_unlock(&Clients_Mutex);
    pthread_create(&clients[index].thread, NULL, handle_chat, static_cast<void *>(&clients[index]));
    printf("[ENTER] new client added, now %d in total\n", connected_clients_num);

    return index;
}

void send_all(Client *client, std::string s)
{
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++)
    {
        if (clients[i].connected && (clients + i) != client)
        {
            int send_len = 0;
            auto mess = s;
            do
            {
                int send_num = send(clients[i].fd, mess.data(), mess.length(), 0); // 本次发送的字符数
                mess = mess.substr(send_num);                                      // 假如本次发了4个字符，就要从原来字符串的第4个字符重新开始发
                send_len = mess.length();                                          // 需要发送的字符数
            } while (send_len);
        }
    }
}

ssize_t receive(Client *client, void *buf, size_t n)
{
    int size = recv(client->fd, buf, n, 0);
    return size;
}

void *handle_chat(void *data)
{
    Client *user = (Client *)data; // 在线程中解引用

    while (1)
    {
        char buffer[MAX_SINGLE_MESSAGE_LENGTH] = "";
        ssize_t len = receive(user, buffer, MAX_SINGLE_MESSAGE_LENGTH);

        if (len <= 0)
            break;

        auto mess_box = split(buffer, "\n");

        // 创建一个新消息
        Message new_message;
        new_message.clip_num = mess_box.second;
        new_message.message_content = mess_box.first;
        new_message.sender_address = user;

        pthread_mutex_lock(&Queue_Mutex);
        message_queue.push(new_message);
        pthread_mutex_unlock(&Queue_Mutex);
    }
    client_destroy(user);
    return NULL;
}

void *handle_send(void *data)
{
    while (true)
    {
        if (message_queue.size())
        {
            Message message_to_be_sent = message_queue.front();
            message_queue.pop();

            auto messes = message_to_be_sent.message_content;
            auto clip_num = message_to_be_sent.clip_num;
            auto user = message_to_be_sent.sender_address;

            for (int i = 0; i < clip_num; i++)
            {
                if (messes[i].length())
                {
                    std::string mess = " [Message]";
                    mess += messes[i];
                    mess += "\n";
                    send_all(user, mess);
                }
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
    if (listen(fd, MAX_CLIENT_NUMBER))
    {
        perror("listen");
        return 1;
    }

    // 初始化所有客户端
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++)
    {
        clients[i].connected = false;
    }
    // 初始化发送线程
    while (message_queue.size())
    {
        message_queue.pop();
    }
    pthread_t message_queue_thread;
    pthread_create(&message_queue_thread, NULL, handle_send, NULL);
    do
    {
        int new_fd = accept(fd, NULL, NULL);
        if (new_fd == -1)
        {
            perror("accept");
            return 0;
        }
        client_add(new_fd);
    } while (connected_clients_num);
    return 0;
}
