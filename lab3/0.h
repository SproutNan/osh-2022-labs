#include <string>
#include <vector>

#ifdef _1_cpp

#define MAX_SINGLE_MESSAGE_LENGTH   1024    // 缓冲区长度

struct Pipe {
    int fd_send;
    int fd_recv;
};

// 按delimiter分割
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

#endif

#ifdef _2_cpp

#define MAX_CLIENT_NUMBER           32
#define MAX_SINGLE_MESSAGE_LENGTH   1024    // 缓冲区大小

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
	pthread_t thread;
} Client;

typedef struct Message {
    Client* sender_address;
    std::vector<std::string> message_content;
    int clip_num;
} Message;

#endif

#ifdef _3_cpp

#define MAX_CLIENT_NUMBER 32
#define MAX_SINGLE_MESSAGE_LENGTH 1025 // 缓冲区

typedef std::pair<bool, int> Client;
typedef struct timeval timev_t;
typedef struct epoll_event ep_evt;

// 按delimiter分割：copied from ta's lab2
std::pair<std::vector<std::string>, int> split(char *str, const std::string &delimiter)
{
    std::string s = str;
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    if (s.length())
    {
        res.push_back(s);
    }
    return std::pair<std::vector<std::string>, int>(res, res.size());
}
#endif

#ifdef _4_cpp

#define MAX_CLIENT_NUMBER 32
#define MAX_SINGLE_MESSAGE_LENGTH 1025 // 缓冲区
#define EPOLL_SIZE MAX_CLIENT_NUMBER

typedef std::pair<bool, int> Client;
typedef struct timeval timev_t;
typedef struct epoll_event ep_evt;

// 按delimiter分割：copied from ta's lab2
std::pair<std::vector<std::string>, int> split(char *str, const std::string &delimiter)
{
    std::string s = str;
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    if (s.length())
    {
        res.push_back(s);
    }
    return std::pair<std::vector<std::string>, int>(res, res.size());
}
#endif

#ifdef _5_cpp

#define MAX_CLIENT_NUMBER 32
#define MAX_SINGLE_MESSAGE_LENGTH 1025 // 缓冲区

// 按delimiter分割，并且传入字符串长度strlen：copied from ta's lab2
std::pair<std::vector<std::string>, int> split(char *str, const std::string &delimiter, int strlen)
{
    std::string s = str;
    s = s.substr(0, strlen);
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    if (s.length())
    {
        res.push_back(s);
    }
    return std::pair<std::vector<std::string>, int>(res, res.size());
}

enum
{
    ACCEPT,
    READ,
    WRITE,
    PROV_BUF,
};

typedef struct conn_info
{
    int fd;
    __u16 type;
    __u16 bid;
} conn_info;

typedef struct Client_uring
{
    int connected;
    int fd;
} Client_uring;

#endif
