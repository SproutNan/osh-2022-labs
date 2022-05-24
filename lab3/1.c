#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_SINGLE_MESSAGE_CLIP_NUM 128     // 单条消息最多段数
#define MAX_SINGLE_MESSAGE_LENGTH   1024    // 单条消息最大长度，超过此条消息将被识别为大消息

struct Pipe {
    int fd_send;
    int fd_recv;
};

// 基于分隔符delimiter对于string做分割，并trim, 返回段数
int split_c(char* string, char* delimiter, char** string_clips) {
    // char string_dup[MAX_SINGLE_MESSAGE_LENGTH];
    string_clips[0] = strtok(string, delimiter);
    int clip_num = 0;

    do {
        char* head, * tail;
        head = string_clips[clip_num];
        tail = head + strlen(string_clips[clip_num]) - 1;
        while (*head == ' ' && head != tail) head++;
        while (*tail == ' ' && tail != head) tail--;
        *(tail + 1) = '\0';
        string_clips[clip_num] = head;
        clip_num++;
    } while (string_clips[clip_num] = strtok(NULL, delimiter));
    return clip_num;
}

void *handle_chat(void *data) {
    struct Pipe *pipe = (struct Pipe *)data; // 在线程中解引用
    char buffer[1024] = "Message:";
    // ssize_t len; // 获得的消息长度
    while (1) {
        ssize_t len = recv(pipe->fd_send, buffer + 8, 1000, 0);
        char* messes[MAX_SINGLE_MESSAGE_CLIP_NUM]; // 等待发送的消息列表
        int clip_num = split_c(buffer, "\n", messes);
        for (int i = 0; i < clip_num; i++) {
            if (strlen(messes[i])) {
                char buffer[1024] = "Message:";
                char* mess = strcat(buffer, messes[i]);
                send(pipe->fd_recv, mess, strlen(mess), 0);
            }
        }
    }
    // while ((len = recv(pipe->fd_send, buffer + 8, 1000, 0)) > 0) {
    //     send(pipe->fd_recv, buffer, len + 8, 0);
    // }
    return NULL;
}

int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }
    if (listen(fd, 2)) {
        perror("listen");
        return 1;
    }
    int fd1 = accept(fd, NULL, NULL);
    int fd2 = accept(fd, NULL, NULL);
    if (fd1 == -1 || fd2 == -1) {
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

