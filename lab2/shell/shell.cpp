#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <vector>

#define MAX_CMDLINE_LENGTH  1024    /* max command_line length in a line*/
#define MAX_BUF_SIZE        4096    /* max buffer size */
#define MAX_CMD_ARG_NUM     32      /* max number of single command args */
#define W_END 1         // pipe write end
#define R_END 0         // pipe read end

bool is_debug = false;
bool is_exit = false;

std::string path1 = "/home/";
std::string path2 = "/.sprout_history";
std::string path3 = "/.sprout_shell_alias/";
std::string path;
std::string cpath;
const int history_max = 1024;

class history {
private:
    std::vector<std::string> history_cmds;
public:
    history() {
        if (load_history_insts() && is_debug) {
            std::cout << "Load history instructions success.\n";
        }
    }
    ~history() {
        if (save_history_insts() && is_debug) {
            std::cout << "Save history instructions success.\n";
        }
    }
    // load history to vector
    bool load_history_insts() {
        // init the history file
        char username[200];
        int _ret = getlogin_r(username, 200);
        if (_ret < 0) {
            return false;
        }
        path = path1 + username + path2;
        cpath = path2 + username + path3;

        auto history_file_re = open(path.data(), O_CREAT | O_APPEND, 0664);
        if (history_file_re < 0) {
            printf("open '%s' error: %s\n", path.data(), strerror(errno));
        }
        close(history_file_re);

        auto history_file = fopen(path.data(), "r");
        std::string history_file_contents;
        while (true) {
            char ch = fgetc(history_file);
            if (ch == EOF) break;
            history_file_contents += ch;
        }
        fclose(history_file);
        // catch split errors
        try {
            history_cmds = split(history_file_contents, "\n");
        }
        catch (...) {
            history_cmds.clear();
            return false;
        }
        return true;
    }
    // when destruct shell, save
    bool save_history_insts() {
        try {
            auto history_file = fopen(path.data(), "w+");
            int size = history_cmds.size();
            for (int i = 0; i < size; i++) {
                fputs(history_cmds[i].data(), history_file);
                if (i != size - 1) fputc('\n', history_file);
            }
            fclose(history_file);
            return true;
        }
        catch (...) {
            return false;
        }
    }
    // add one cmd to history
    void add_history_inst(std::string s) {
        if (history_cmds.size() >= history_max) {
            std::cout << "history log is too large, try [history -d] to remove all logs.\n";
        }
        else history_cmds.push_back(s);
    }
    void rm_history_inst() {
        history_cmds.clear();
    }
    void disp_history_inst() {
        int size = history_cmds.size();
        for (int i = 0; i < size; i++) {
            std::cout << i + 1 << "\t" << history_cmds[i] << '\n';
        }
    }
    void disp_history_inst(int n) {
        int size = history_cmds.size();
        for (int i = n - 1; i >= 0; i--) {
            std::cout << size - i << "\t" << history_cmds[size - i - 1] << '\n';
        }
    }
    std::string get_history_by_number(int number = INT_MAX){
        if (number == INT_MAX) {
            return *history_cmds.rbegin();
        }
        else if (number <= history_cmds.size()) {
            return history_cmds[number-1];
        }
        return std::string("");
    }
};

history* history_ptr;

// 基于分隔符delimiter对于string做分割，并trim, 返回段数
int split_c(char* string, char* delimiter, char** string_clips) {
    char string_dup[MAX_BUF_SIZE];
    string_clips[0] = strtok(string, delimiter);
    int clip_num = 0;

    do {
        char* head, * tail;
        head = string_clips[clip_num];
        tail = head + strlen(string_clips[clip_num]) - 1;
        while (*head == ' ' && head != tail)
            head++;
        while (*tail == ' ' && tail != head)
            tail--;
        *(tail + 1) = '\0';
        string_clips[clip_num] = head;
        clip_num++;
    } while (string_clips[clip_num] = strtok(NULL, delimiter));
    return clip_num;
}

// 执行内置命令
int exec_builtin(int argc, char** argv) {
    if (argc == 0) {
        return 0;
    }
    if (strcmp(argv[0], "cd") == 0) {
        if (argc <= 1) {
            std::cout << "Insufficient arguments.\n";
        }

        int _ret = chdir(argv[1]);
        if (_ret < 0) {
            std::cout << "cd failed.\n";
        }
        return 0;
    }
    else if (strcmp(argv[0], "pwd") == 0) {
        // 预先分配好空间
        std::string cwd;
        cwd.resize(PATH_MAX);
        const char* _ret = getcwd(&cwd[0], PATH_MAX);
        if (_ret == nullptr) {
            std::cout << "cwd failed.\n";
        }
        else {
            std::cout << _ret << "\n";
        }
        return 0;
    }
    else if (strcmp(argv[0], "export") == 0) {
        for (int i = 1; i < argc; i++) {
            std::string cmd = argv[i];
            std::string key;
            std::string value;

            size_t pos;
            if ((pos = cmd.find('=')) != std::string::npos) {
                key = cmd.substr(0, pos);
                value = cmd.substr(pos + 1);
            }

            int _ret = setenv(key.data(), value.data(), 1);
            if (_ret < 0) {
                std::cout << "export failed.\n";
            }
        }
        return 0;
    }
    else if (strcmp(argv[0], "exit") == 0) {
        int code = 0;
        if (argc > 1) {
            // atoi
            try {
                code = atoi(argv[1]);
            }
            catch (...) {
                std::cout << "invalid parameter.\n";
                return 0;
            }
        }
        history_ptr->~history();
        exit(code);
        return 0;
    }
    else if (strcmp(argv[0], "history") == 0) {
        if (argc <= 1) {
            history_ptr->disp_history_inst();
        }
        else if (strcmp(argv[1], "-d") == 0) {
            // 命令是 history -d
            if (argc <= 2) {
                history_ptr->rm_history_inst();
            }
            else {
                std::cout << "invalid num of parameter.\n";
            }
        }
        else if (argc <= 2) {
            // 命令是 history n
            int _num = 0;
            try {
                _num = atoi(argv[1]);
                history_ptr->disp_history_inst(_num);
            }
            catch (...) {
                std::cout << "invalid parameter.\n";
            }
        }
        return 0;
    }
    else if (strcmp(argv[0], "echo") == 0) {
        if (argc <= 1) {
            std::cout << '\n';
        }
        else {
            if (strcmp(argv[1], "~root") == 0) {
                std::cout << "/root" << '\n';
                return 0;
            }
            std::string mess = argv[1];
            if (mess.find("$HOME") != mess.npos) {
                while (mess.find("$HOME") != mess.npos) {
                    int pos = mess.find("$HOME");
                    char* home = getenv("HOME");
                    std::string home_s = home;
                    mess = mess.substr(0, pos) + home_s + mess.substr(pos + 5);
                }
                std::cout << mess << '\n';
                return 0;
            }
            else if (mess.find("$SHELL") != mess.npos) {
                while (mess.find("$SHELL") != mess.npos) {
                    int pos = mess.find("$SHELL");
                    char* home = getenv("SHELL");
                    std::string home_s = home;
                    mess = mess.substr(0, pos) + home_s + mess.substr(pos + 6);
                }
                std::cout << mess << '\n';
                return 0;
            }
            else {
                std::cout << mess << '\n';
                return 0;
            }
        }
        return 0;
    }
    else if (strcmp(argv[argc - 1], "env") == 0) {
        for (int i = 0; i < argc - 1; i++) {
            std::string cmd = argv[i];
            std::string key;
            std::string value;

            size_t pos;
            if ((pos = cmd.find('=')) != std::string::npos) {
                key = cmd.substr(0, pos);
                value = cmd.substr(pos + 1);
            }

            int _ret = setenv(key.data(), value.data(), 1);
            if (_ret < 0) {
                std::cout << "export failed.\n";
            }
        }
        return 0;
    }
    else {
        // 不是内置指令
        return -1;
    }
}

// 执行重定向，返回处理过重定向后命令的参数个数
int process_redirect(int argc, char** argv, int* fd) {
    // 默认输入输出为：输入STDIN_FILENO，输出STDOUT_FILENO
    fd[R_END] = STDIN_FILENO;
    fd[W_END] = STDOUT_FILENO;
    int i = 0, j = 0;
    while (i < argc) {
        int tfd;
        std::string s = argv[i];
        // 输出重定向：覆盖
        if (strcmp(argv[i], ">") == 0) {
            tfd = open(argv[i + 1], O_RDWR | O_CREAT | O_TRUNC, 0664);
            if (tfd < 0) {
                printf("open '%s' error: %s\n", argv[i + 1], strerror(errno));
            }
            else {
                fd[W_END] = tfd;
            }
            i += 2;
        }
        // 输出重定向：追加
        else if (strcmp(argv[i], ">>") == 0) {
            tfd = open(argv[i + 1], O_RDWR | O_CREAT | O_APPEND);
            if (tfd < 0) {
                printf("open '%s' error: %s\n", argv[i + 1], strerror(errno));
            }
            else {
                fd[W_END] = tfd;
            }
            i += 2;
        }
        // 输入重定向
        else if (strcmp(argv[i], "<") == 0) {
            tfd = open(argv[i + 1], O_RDONLY);
            if (tfd < 0) {
                printf("open '%s' error: %s\n", argv[i + 1], strerror(errno));
            }
            else {
                fd[R_END] = tfd;
            }
            i += 2;
        }
        // 选做
        else if (s[s.size() - 1] == '<' && s.length() > 1) {
            // 形如 cmd fd< test.txt
            auto fd_s = s.substr(0, s.length() - 1);
            int fd_i;
            try {
                fd_i = atoi(fd_s.data());
            }
            catch (...) {
                std::cout << "invalid redirect parameter.\n";
                return 0;
            }
            // 输入重定向
            tfd = open(argv[i + 1], O_RDONLY);
            if (tfd < 0) {
                printf("open '%s' error: %s\n", argv[i + 1], strerror(errno));
            }
            else {
                fd[R_END] = tfd;
                fd[W_END] = fd_i;
            }
            i += 2;
        }
        else if (s[s.size() - 1] == '>' && s.length() > 1) {
            // 形如 cmd fd> test.txt
            auto fd_s = s.substr(0, s.length() - 1);
            int fd_i;
            try {
                fd_i = atoi(fd_s.data());
            }
            catch (...) {
                std::cout << "invalid redirect parameter.\n";
                return 0;
            }
            // 输入重定向
            tfd = open(argv[i + 1], O_RDWR | O_CREAT | O_TRUNC, 0664);
            if (tfd < 0) {
                printf("open '%s' error: %s\n", argv[i + 1], strerror(errno));
            }
            else {
                fd[W_END] = tfd;
                fd[R_END] = fd_i;
            }
            i += 2;
        }
        else if (s.find(">&") != s.npos && s.length() > 2 && s.find(">&") != s.size() - 2) {
            // 形如 cmd fd1>&fd2 > test.txt
            int pos = s.find(">&");
            auto fd_s1 = s.substr(0, pos);
            auto fd_s2 = s.substr(pos + 2);
            int fd_1, fd_2;
            try {
                fd_1 = atoi(fd_s1.data());
                fd_2 = atoi(fd_s1.data());
            }
            catch (...) {
                std::cout << "invalid redirect parameter.\n";
                return 0;
            }
            fd[W_END] = fd_2;
            fd[R_END] = fd_1;
            i += 1;
        }
        // 没有重定向
        else {
            argv[j++] = argv[i++];
        }
        
    }
    argv[j] = NULL;
    return j;   // 新的argc
}



// 在本进程中执行，且执行完毕后结束进程。
int execute(int argc, char** argv) {
    int fd[2];
    // 默认输入输出到命令行，即输入STDIN_FILENO，输出STDOUT_FILENO 
    fd[R_END] = STDIN_FILENO;
    fd[W_END] = STDOUT_FILENO;
    // 处理重定向符，如果不做本部分内容，请注释掉process_redirect的调用
    argc = process_redirect(argc, argv, fd);
    if (exec_builtin(argc, argv) == 0) {
        exit(0);
    }
    // 将标准输入输出STDIN_FILENO和STDOUT_FILENO修改为fd对应的文件
    dup2(fd[R_END], STDIN_FILENO);
    dup2(fd[W_END], STDOUT_FILENO);
    execvp(argv[0], argv);
    return 0;
}

// 打印prompt
void prompt() {
    char current_path[200];
    getcwd(current_path, 200);

    std::cout << "\033[32m" << current_path << "\033[34m" << " # \033[0m";
}

// sigint时的处理
void sigint_exit(int exit_code) {
    is_exit = true;
    int status;
    int ret = waitpid(0, &status, WNOHANG);
    if (ret == -1) {
        std::cout << '\n';
        prompt();
        fflush(stdout);
    }
    return;
}

int main() {
    // 处理ctrl+c
    signal(SIGINT, sigint_exit);
    // 预处理history
    history_ptr = new history;
    // 输入的命令
    char command_line[MAX_CMDLINE_LENGTH];
    char* complete_commands[128];
    char* commands[128];
    while (true) {
        int _status;
        signal(SIGINT, sigint_exit);
        prompt();

        fflush(stdin);
        fflush(stdout);

        fgets(command_line, 256, stdin);
        strtok(command_line, "\n");

        std::string backup_command = command_line; // std::string 备份
        if (backup_command[backup_command.size() - 1] == '\n') 
            backup_command = backup_command.substr(0, backup_command.length() - 1); // 去除备份末尾的\n

        if (strcmp(command_line, "!!") == 0) {
            strcpy(command_line, history_ptr->get_history_by_number().data()); // 如果cmdline是!!，则把cmdlin换成history最后一条
        }
        else if (command_line[0] == '!') { // 如果是 !num，先转换后者
            int number = INT_MIN;
            try {
                number = atoi(split(backup_command.substr(1), " ")[0].data());
            }
            catch (...) {
                number = INT_MIN;
                std::cout << "invalid instruction.\n";
                continue;
            }
            strcpy(command_line, history_ptr->get_history_by_number(number).data()); // 把cmdlin换成history第number条
        }

        backup_command = command_line;
        if (backup_command[backup_command.size() - 1] == '\n') 
            backup_command = backup_command.substr(0, backup_command.length() - 1); // 更新备份

        // ; 的处理
        int count = split_c(command_line, ";", complete_commands);
        for (int j = 0; j < count; j++) {
            // 不接受后面的指令了
            if (is_exit) {
                is_exit = false;
                break;
            }
            // 按|拆分
            int cmd_count = split_c(complete_commands[j], "|", commands);
            if (cmd_count == 0) {
                continue;
            }
            else if (cmd_count == 1) {     // 没有管道的单一命令
                char* argv[MAX_CMD_ARG_NUM];
                int argc;
                int fd[2];
                // 处理参数
                argc = split_c(commands[0], " ", argv);
                // 在没有管道时，内建命令直接在主进程中完成，外部命令通过创建子进程完成
                if (exec_builtin(argc, argv) == 0) {
                    continue;
                }
                // 创建子进程，运行命令，等待命令运行结束
                pid_t pid;
                pid = fork();
                if (pid == 0) {
                    //子进程
                    if (execute(argc, argv) != 0) {
                        exit(-1);
                    }
                }
                else if (pid > 0) {
                    wait(NULL);
                }

            }
            else {    
                // 有管道
                int read_fd;    // 上一个管道的读端口（出口）
                for (int i = 0; i < cmd_count; i++) {
                    int pipefd[2];
                    // 创建管道，n条命令只需要n-1个管道，所以有一次循环中是不用创建管道的
                    if (i < cmd_count - 1) {
                        int ret = pipe(pipefd);
                        if (ret < 0) {
                            printf("pipe error!\n");
                            continue;
                        }
                    }
                    int pid = fork();
                    if (pid == 0) {
                        char* argv[MAX_CMD_ARG_NUM];
                        int argc = split_c(commands[i], " ", argv);
                        // 除了最后一条命令外，都将标准输出重定向到当前管道入口
                        if (i < cmd_count - 1) {
                            dup2(pipefd[1], STDOUT_FILENO);
                        }
                        // 除了第一条命令外，都将标准输入重定向到上一个管道出口
                        if (i > 0) {
                            dup2(read_fd, STDIN_FILENO);
                        }
                        // 处理参数，分出命令名和参数，并使用execute运行
                        // 在使用管道时，为了可以并发运行，所以内建命令也在子进程中运行
                        execute(argc, argv);
                        exit(255);
                    }
                    // 父进程除了第一条命令，都需要关闭当前命令用完的上一个管道读端口
                    // 父进程除了最后一条命令，都需要保存当前命令的管道读端口
                    // 关闭父进程没用的管道写端口
                    // 管道并发执行，不在每个子进程结束后才运行下一个，而是直接创建下一个子进程
                    if (pid > 0) {
                        while (wait(NULL) > 0) {
                            if (i > 0) close(read_fd);
                            if (i < cmd_count) read_fd = pipefd[0];
                            close(pipefd[W_END]);
                        }
                    }
                }
            }
        }
        history_ptr->add_history_inst(backup_command);
    }
}
