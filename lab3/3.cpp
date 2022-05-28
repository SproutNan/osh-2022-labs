#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
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


std::pair<bool, int> clients_list[MAX_CLIENT_NUMBER];
int connected_clients_num = 0;
struct timeval timeout = {0, 500};

void display_client() {
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
        putchar(clients_list[i].first ? '1' : '0');
    }
    putchar('\n');
}
void client_destroy(std::pair<bool, int>* user) {
    user->first = false;
    close(user->second);
    connected_clients_num--;
    printf("Client left, %d in total\n", connected_clients_num);
}

int client_add(int fd) {
    int index = -1;
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
        if (!clients_list[i].first) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    clients_list[index].first = true;
    clients_list[index].second = fd;
    connected_clients_num++;
	
    printf("New client entered, %d in total\n", connected_clients_num);

	return index;
}

void send_all(std::pair<bool, int>* client, std::string s) {
	for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
		if (clients_list[i].first && &clients_list[i] != client) {
            int send_len = 1;
            auto mess = s;
            do {
                int send_num = send(clients_list[i].second, mess.data(), mess.length(), 0); // 本次发送的字符数
                mess = mess.substr(send_num); // 假如本次发了4个字符，就要从原来字符串的第4个字符重新开始发
                send_len = mess.length(); // 需要发送的字符数
            } while (send_len);
		}
	}
}

ssize_t receive(std::pair<bool, int>* client, void *buf, size_t n) {
	int size = recv(client->second, buf, n, 0);
	return size;
}

int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == 0) {
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

    fd_set clients; // 需要监视的clients集合
    // 初始化所有客户端
    for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
        clients_list[i].first = false;
    }
    while (true) {
        int new_fd = accept(fd, NULL, NULL);
		if (new_fd >= 0) {
            client_add(new_fd);
		}
        FD_ZERO(&clients); // 初始化

        int upper_bound = 0;
        for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
            if (clients_list[i].first) {
                if (clients_list[i].second > upper_bound) {
                    upper_bound = clients_list[i].second;
                }
                FD_SET(clients_list[i].second, &clients);
            }
        }

        int select_ret = select(upper_bound + 1, &clients, NULL, NULL, &timeout);
        if (select_ret > 0) {
            for (int i = 0; i < MAX_CLIENT_NUMBER; i++) {
                if (clients_list[i].first) {
                    if (FD_ISSET(clients_list[i].second, &clients)) {
                        // ready
                        while (true) { 
                            char buffer[MAX_SINGLE_MESSAGE_LENGTH] = "";
                            ssize_t len = receive(&clients_list[i], buffer, MAX_SINGLE_MESSAGE_LENGTH);

                            if (len <= 0) {
                                break;
                            }

                            if (strcmp(buffer, "quit()\n") == 0) {
                                client_destroy(&clients_list[i]);
                                break;
                            }
                            
                            auto mess_box = split(buffer, "\n");
                            auto messes = mess_box.first;
                            auto clip_num = mess_box.second;

                            for (int j = 0; j < clip_num; j++) {
                                // if (j == clip_num - 1 && *(messes.rbegin()->rbegin()) == EOF) {
                                //     printf("find a eof, replace by '!'");
                                //     messes[j][messes[j].length()-1] = '!';
                                // }
                                if (messes[j].length()) {
                                    char sender_address[100];
                                    sprintf(sender_address, "%p", &clients_list[i]);
                                    std::string mess = " [Message from ";
                                    mess += sender_address;
                                    mess += "]";
                                    mess += messes[j];
                                    mess += "\n";
                                    send_all(&clients_list[i], mess);
                                    // display_client();
                                }
                            }
                        }
                        fcntl(clients_list[i].second, F_SETFL, fcntl(clients_list[i].second, F_GETFL, 0) | O_NONBLOCK);
                    }
                }
            }
        }
        else if (select_ret == 0) continue;
        else break;
    }
	return 0;
}
