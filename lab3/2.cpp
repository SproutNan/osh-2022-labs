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

#define MAX_CLIENT_NUMBER           32
#define MAX_SINGLE_MESSAGE_LENGTH   1024    // 单条消息最大长度，超过此条消息将被识别为大消息

// 按delimiter分割：copied from ta's lab2
std::pair<std::vector<std::string>, int> split(char* str, const std::string &delimiter) {
    std::string s = str;
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    if (s.length()) {
        res.push_back(s);
    }
    return std::pair<std::vector<std::string>, int>(res, res.size());
}

typedef struct Client {
    bool connected;
    int fd;
	// pthread_mutex_t mutex;
	pthread_t thread;
} Client;

typedef struct Message {
    Client* sender_address;
    std::vector<std::string> message_content;
    int clip_num;
} Message;

void* handle_chat(void *data);

std::queue<Message> message_queue;
Client clients[MAX_CLIENT_NUMBER];
int valid_clients_num = 0;

pthread_mutex_t ClientsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t QueueMutex = PTHREAD_MUTEX_INITIALIZER;

void client_destroy(Client* user) {
    pthread_mutex_lock(&ClientsMutex);
    user->connected = false;
    close(user->fd);
    valid_clients_num--;
    // pthread_mutex_destroy(&user->mutex);
    pthread_mutex_unlock(&ClientsMutex);

    printf("Client left, %d in total\n", valid_clients_num);
}

int client_add(int fd) {
    int index = -1;
    pthread_mutex_lock(&ClientsMutex);
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
        if (!clients[i].connected) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        pthread_mutex_unlock(&ClientsMutex);
        return -1;
    }
    clients[index].connected = true;
    clients[index].fd = fd;
    valid_clients_num++;
	pthread_mutex_unlock(&ClientsMutex);
	// pthread_mutex_init(&clients[index].mutex, NULL);
	pthread_create(&clients[index].thread, NULL, handle_chat, clients + index);
	
    printf("New client entered, %d in total\n", valid_clients_num);

	return index;
}

ssize_t send_to_all(Client *client, std::string s, size_t n) {
	int size = 0;
	for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
		if (clients[i].connected && (clients + i) != client) {
			int tmp_size = send(clients[i].fd, s.data(), n, 0);
			printf("Send %d bytes to client %d\n", tmp_size, i);
			if (size == 0) {
				size = tmp_size;
			}
			else if (tmp_size != size) {
				perror("Inconsistent size of data sent");
			}
		}
	}
	return size;
}

ssize_t receive(Client *client, void *buf, size_t n) {
	int size = recv(client->fd, buf, n, 0);
	return size;
}

void* handle_chat(void* data) {
    Client* user = (Client*) data; // 在线程中解引用

    while (1) {
        char buffer[MAX_SINGLE_MESSAGE_LENGTH] = "";
        ssize_t len = receive(user, buffer, MAX_SINGLE_MESSAGE_LENGTH);
        // send(pipe->fd_recv, std::to_string(len).data(), std::to_string(len).length(), 0);

        if (len <= 0) break;

        auto mess_box = split(buffer, "\n");

        // 创建一个新消息
        Message new_message;
        new_message.clip_num = mess_box.second;
        new_message.message_content = mess_box.first;
        new_message.sender_address = user;

        pthread_mutex_lock(&QueueMutex);
        message_queue.push(new_message);
        pthread_mutex_unlock(&QueueMutex);
    }
    client_destroy(user);
    return NULL;
}

void* handle_send(void* data) {
    while (true) {
        if (message_queue.size()) {
            Message message_to_be_sent = message_queue.front();
            message_queue.pop();

            auto messes = message_to_be_sent.message_content;
            auto clip_num = message_to_be_sent.clip_num;
            auto user = message_to_be_sent.sender_address;

            for (int i = 0; i < clip_num; i++) {
                if (messes[i].length()) {
                    std::string mess = " [Message]";
                    mess += messes[i];
                    mess += "\n";
                    int send_len = 0;
                    do {
                        int send_num = send_to_all(user, mess, mess.length()); // 本次发送的字符数
                        mess = mess.substr(send_num); // 假如本次发了4个字符，就要从原来字符串的第4个字符重新开始发
                        send_len = mess.length(); // 需要发送的字符数
                    } while (send_len);
                }
            }

        }
    }
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
    if (listen(fd, MAX_CLIENT_NUMBER)) {
        perror("listen");
        return 1;
    }

    // 初始化所有客户端
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
        clients[i].connected = false;
    }
    // 初始化发送线程
    while (message_queue.size()) {
        message_queue.pop();
    }
    pthread_t message_queue_thread;
    pthread_create(&message_queue_thread, NULL, handle_send, NULL);
	do {
		int new_fd = accept(fd, NULL, NULL);
		if (new_fd == -1) {
			perror("accept");
			return 0;
		}
		client_add(new_fd);
	} while (valid_clients_num);
	return 0;
}
