#include "shell_new.h"

constexpr auto history_path = "./.n_shell_history";
constexpr auto history_max = 100;
constexpr auto W_END = 1;
constexpr auto R_END = 0;

std::vector<std::string> history;
std::vector<std::string> cmd_multiline; // when a cmd contains ";", split it to atom lines and store to this vector
int cmd_multiline_iterator = -1;
int cmd_pipedline_iterator = -1;

std::vector<std::string> cmd_pipedline; // when a atom line contains "|", it uses pipe, split it to COUNT("|")+1 quarks

enum get_quark_cmd_enum : int { HEAD, INNER, TAIL, ERROR };
enum inst_type : int { BUILDIN, OUTER };

bool load_history_insts(); // load history to vector
bool save_history_insts(); // when destruct shell, save
void add_history_inst(std::string s);
void rm_history_inst(int n);
void disp_history_inst();
void disp_history_inst(int n);
// void command_preprocessing(std::string& s); // fill cmd_multiline
// void fill_pipedline(std::string& s);
// bool get_atom_cmd();
// std::pair<std::string, int> get_quark_cmd(); // int: get_quark_cmd_enum

// bool execuate_buildin(int argc, std::vector<std::string>& argv, int* fd);
// bool execuate(int argc, std::vector<std::string>& argv);
// int redirect(int argc, std::vector<std::string>& argv, int* fd);

class history_f {
public:
    history_f() {
        // load history commands to shell
        if (load_history_insts()) {
            std::cout << "Success loaded history\n";
        }
        else std::cout << "Failed loaded history\n";
        // end load history commands
    }
    ~history_f() {
        save_history_insts();
    }
};

int redirect(int argc, std::vector<std::string>& argv, int* fd) {
    fd[R_END] = STDIN_FILENO;
    fd[W_END] = STDOUT_FILENO;
    if (argv[argc - 2] == ">" || argv[argc - 2] == ">>") {
        const char w[3] = "w+";
        const char a[3] = "a+";
        auto type = (argv[argc - 2] == ">") ? w : a;
        auto target = fopen(argv[argc - 1].c_str(), type);
        if (fileno(target) < 0) {
            std::cout << "open [" << argv[argc - 1] << "] failed" << '\n';
        }
        else {
            fd[W_END] = fileno(target);
        }
        argv.pop_back();
        argv.pop_back();
        return argc - 2;
    }
    else if (argv[argc - 2] == "<") {
        auto target = fopen(argv[argc - 1].c_str(), "r+");
        if (fileno(target) < 0) {
            std::cout << "open [" << argv[argc - 1] << "] failed" << '\n';
        }
        else {
            fd[R_END] = fileno(target);
        }
        argv.pop_back();
        argv.pop_back();
        return argc - 2;
    }
    else {
        return argc;
    }
}

int execuate_buildin(int argc, std::vector<std::string>& argv, int* fd) {
    // return: fail or succeed
    // no execuable cmd
    if (argv.empty()) {
        return 0;
    }

    // cd
    if (argv[0] == "cd") {
        if (argv.size() <= 1) {
            // dont use std::endl，std::endl = "\n" + fflush(stdout)
            std::cout << "Insufficient arguments\n";
        }

        int _ret = chdir(argv[1].c_str());
        if (_ret < 0) {
            std::cout << "cd failed\n";
        }
        return 0;
    }

    // pwd
    if (argv[0] == "pwd") {
        std::string cwd;
        // pre-allocate
        cwd.resize(PATH_MAX);

        const char* _ret = getcwd(&cwd[0], PATH_MAX);
        if (_ret == nullptr) {
            std::cout << "cwd failed\n";
        }
        else {
            std::cout << _ret << "\n";
        }
        return 0;
    }

    // set env
    if (argv[0] == "export") {
        for (auto i = ++argv.begin(); i != argv.end(); i++) {
            std::string key = *i;

            std::string value;
            size_t pos;
            if ((pos = i->find('=')) != std::string::npos) {
                key = i->substr(0, pos);
                value = i->substr(pos + 1);
            }

            int _ret = setenv(key.data(), value.data(), 1);
            if (_ret < 0) {
                std::cout << "export $" << value << "$ failed\n";
                return 0;
            }
        }
        return 0;
    }

    // exit
    if (argv[0] == "exit") {
        if (argv.size() <= 1) {
            exit(0);
        }
        // std::string 2 int
        std::stringstream _code_stream(argv[1]);
        int _code = 0;
        _code_stream >> _code;

        if (!_code_stream.eof() || _code_stream.fail()) {
            std::cout << "invalid exit code\n";
            return 0;
        }
        exit(_code);
        return 0;
    }

    // history
    if (argv[0] == "history") {
        if (argv.size() <= 1) {
            disp_history_inst();
        }
        else {
            if (argv[1] == "-d") {
                if (argv.size() <= 2) {
                    rm_history_inst(history_max);
                }
                else {
                    // std::string 2 int
                    std::stringstream _code_stream(argv[2]);
                    int _code = 0;
                    _code_stream >> _code;

                    if (!_code_stream.eof() || _code_stream.fail()) {
                        std::cout << "invalid oprand\n";
                        return 0;
                    }
                    rm_history_inst(_code);
                }
            }
            else {
                // std::string 2 int
                std::stringstream _code_stream(argv[1]);
                int _code = 0;
                _code_stream >> _code;

                if (!_code_stream.eof() || _code_stream.fail()) {
                    std::cout << "invalid oprand\n";
                    return 0;
                }
                disp_history_inst(_code);
            }
        }
        return 0;
    }

    return -1;
}

int execuate(int argc, std::vector<std::string>& argv) {
    // return: fail or succeed
    int _fd[2];
    _fd[R_END] = STDIN_FILENO;
    _fd[W_END] = STDOUT_FILENO; //default settings

    argc = redirect(argc, argv, _fd);

    if (execuate_buildin(argc, argv, _fd) == 0) exit(0);
    // outer inst
    dup2(_fd[R_END], STDIN_FILENO);
    dup2(_fd[W_END], STDOUT_FILENO);

    char* arg_ptrs[argv.size() + 1];
    for (auto i = 0; i < argv.size(); i++) {
        arg_ptrs[i] = &argv[i][0];
    }
    // exec-p argv should end with nullptr
    arg_ptrs[argv.size()] = nullptr;

    execvp(argv[0].c_str(), arg_ptrs);
}

bool load_history_insts() {
    auto history_file = fopen(history_path, "a");
    fclose(history_file);
    // init the history file
    history_file = fopen(history_path, "r");
    std::string history_file_contents;
    while (true) {
        char ch = fgetc(history_file);
        if (ch == EOF) break;
        history_file_contents += ch;
    }
    fclose(history_file);
    // catch split errors
    try {
        history = split(history_file_contents, "\n");
    }
    catch (...) {
        history.clear();
        return false;
    }
    return true;
}

bool save_history_insts() {
    auto history_file = fopen(history_path, "w+");
    int size = history.size();
    for (int i = 0; i < size; i++) {
        fputs(history[i].data(), history_file);
        if (i != size - 1) fputc('\n', history_file);
    }
    fclose(history_file);
    return true;
}

void command_preprocessing(std::string& s) {
    cmd_multiline_iterator = -1;
    cmd_multiline.clear();
    auto atoms = split(s, ";");
    for (auto& a : atoms) {
        if (a == "!!") {
            cmd_multiline.push_back(TrimOmit(history[history.size() - 1]));
        }
        else if (a[0] == '!') {
            // std::string 2 int
            std::stringstream _code_stream(a.substr(1));
            int _code = 0;
            _code_stream >> _code;

            if (!_code_stream.eof() || _code_stream.fail()) {
                std::cout << "history oprand exceed\n";
                continue;
            }

            cmd_multiline.push_back(TrimOmit(history[_code - 1]));
        }
        else cmd_multiline.push_back(TrimOmit(a));
    }
}

void fill_pipedline(std::string& s) {
    cmd_pipedline_iterator = -1;
    cmd_pipedline.clear();
    auto quarks = split(s, "|");
    for (auto& q : quarks) {
        cmd_pipedline.push_back(TrimOmit(q));
    }
}

void add_history_inst(std::string s) {
    if (history.size() >= history_max) {
        std::cout << "history log is too large, try [history -d n] to remove old n logs.\n";
    }
    else history.push_back(s);
}

void rm_history_inst(int n) {
    for (auto i = 0; i < n; i++) {
        if (history.size()) history.pop_back();
    }
}

void disp_history_inst() {
    int size = history.size();
    for (int i = 0; i < size; i++) {
        std::cout << i + 1 << "\t" << history[i] << '\n';
    }
}

void disp_history_inst(int n) {
    int size = history.size();
    for (int i = n - 1; i >= 0; i--) {
        std::cout << size - i << "\t" << history[size - i] << '\n';
    }
}

bool get_atom_cmd() {
    if (cmd_multiline_iterator + 1 < cmd_multiline.size()) {
        fill_pipedline(cmd_multiline[cmd_multiline_iterator + 1]);
        cmd_multiline_iterator++;
        return true;
    }
    else return false;
}

std::pair<std::string, int> get_quark_cmd() {
    int _deux = INNER;
    if (cmd_pipedline_iterator == -1) _deux = HEAD;
    else if (cmd_pipedline_iterator == cmd_pipedline.size() - 2) _deux = TAIL;
    else if (cmd_pipedline_iterator >= cmd_pipedline.size() - 1) _deux = ERROR;

    auto _s = cmd_pipedline[cmd_pipedline_iterator + 1];
    cmd_pipedline_iterator++;
    return std::pair<std::string, int>(_s, _deux);
}

int main() {
    // 不同步 iostream 和 cstdio 的 buffer
    std::ios::sync_with_stdio(false);

    history_f history_inst;

    std::string cmd;
    while (true) {
        std::cout << "# ";
        // 读入一行。std::getline 结果不包含换行符。
        std::getline(std::cin, cmd);
        command_preprocessing(cmd);
        if (cmd.find('!') == cmd.npos) add_history_inst(cmd); // add cmd to history
        // move one atom cmd to pipeline slot
        while (get_atom_cmd()) {
            auto pipecmd_num = cmd_pipedline.size();

            if (pipecmd_num == 0) {
                continue;
            }

            else if (pipecmd_num == 1) {
                // no pipe, running in father process
                auto s = get_quark_cmd().first;
                auto argv = split(s, " ");
                auto argc = argv.size();
                int fd[2];

                if (execuate_buildin(argc, argv, fd) == 0) continue;

                pid_t pid;
                pid = fork();
                if (pid == 0) {
                    //子进程（紫禁城！）
                    if (execuate(argc, argv) != 0) {
                        exit(-1);
                    }
                }
                else if (pid > 0) {
                    wait(NULL);
                }

            }

            else if (pipecmd_num == 2) {
                int pipefd[2];
                int ret = pipe(pipefd);
                if (ret < 0) {
                    printf("pipe error!\n");
                    continue;
                }
                // 子进程1
                int pid = fork();
                if (pid == 0) {
                    /*TODO:子进程1 将标准输出重定向到管道，注意这里数组的下标被挖空了要补全*/
                    /*子进程 1 中将输出重定向到管道，
                    先关闭管道输出，用 dup2 修改子进程输出到管道输入后，
                    关闭管道输入。*/
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                    /*
                        在使用管道时，为了可以并发运行，所以内建命令也在子进程中运行
                        因此我们用了一个封装好的execute函数
                     */
                    auto argv = split(get_quark_cmd().first, " ");
                    auto argc = argv.size();

                    execuate(argc, argv);
                    exit(255);

                }
                // 因为在shell的设计中，管道是并发执行的，所以我们不在每个子进程结束后才运行下一个
                // 而是直接创建下一个子进程
                // 子进程2
                pid = fork();
                if (pid == 0) {
                    /* TODO:子进程2 将标准输入重定向到管道，注意这里数组的下标被挖空了要补全 */
                    /*子进程 2 中将输入重定向到管道，
                    先关闭管道输出，用 dup2 修改子进程输入到管道输出后，
                    关闭管道输出。*/
                    close(pipefd[1]);
                    dup2(pipefd[0], STDIN_FILENO);
                    close(pipefd[0]);

                    auto argv = split(get_quark_cmd().first, " ");
                    auto argc = argv.size();
                    
                    execuate(argc, argv);
                    exit(255);
                }
                close(pipefd[W_END]);
                close(pipefd[R_END]);


                while (wait(NULL) > 0);
            } 

            else {
                int last_read_end;
                for (int i = 0; i < pipecmd_num; i++) {
                    int pipe_fd[2];
                    if (i != pipecmd_num - 1) {
                        auto _ret = pipe(pipe_fd);
                        if (_ret < 0) {
                            std::cout << "pipe error\n";
                            continue;
                        }
                    }
                    auto new_pid = fork();
                    if (new_pid == 0) {
                        // dup stdout to pipe w_end except for rbegin 
                        if (i != pipecmd_num - 1) {
                            dup2(pipe_fd[W_END], STDOUT_FILENO);
                        }
                        // dup stdin to pipe r_end except for begin
                        if (i != 0) {
                            dup2(last_read_end, STDIN_FILENO);
                        }

                        auto s = get_quark_cmd().first;
                        // split to args
                        auto argv = split(s, " ");
                        auto argc = argv.size();

                        execuate(argc, argv);
                        exit(255);
                    }
                    else {
                        while (wait(nullptr) > 0) {
                            if (i != 0) {
                                close(last_read_end);
                            }
                            if (i != pipecmd_num - 1) {
                                last_read_end = pipe_fd[R_END];
                            }
                            close(pipe_fd[W_END]);
                        }
                    }
                }
            }
        }
    }
}