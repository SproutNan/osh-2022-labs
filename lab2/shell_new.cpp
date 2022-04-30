#include "shell_new.h"

constexpr auto history_path = "./.n_shell_history";

class shell {
private:
    // when established, load history to this vector
    std::vector<std::string> history;

public:
    shell();
    ~shell();
    bool load_history_insts(); // load history to vector
    bool save_history_insts(); // when destruct shell, save
    void run(); // one turn in execuate, 
    // TODO: add run instruction to history file
    bool contains_pipe(std::string& s);
};

bool shell::load_history_insts() {
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
        this->history = split(history_file_contents, "\n");
    }
    catch (...) {
        this->history.clear();
        return false;
    }
    return true;
}

bool shell::save_history_insts() {
    auto history_file = fopen(history_path, "w+");
    int size = history.size();
    for (int i = 0; i < size; i++) {
        fputs(history[i].data(), history_file);
        if (i != size - 1) fputc('\n', history_file);
    }
    fclose(history_file);
}

bool shell::contains_pipe(std::string& s) {
    return s.find("|") != s.npos;
}

shell::shell() {
    if (load_history_insts()) {
        std::cout << "Success loaded history\n";
        int size = this->history.size();
        for (int i = 0; i < size; i++) {
            std::cout << i+1 << " " << history[i] << '\n';
        }
    }
    else std::cout << "Failed loaded history\n";
}

shell::~shell() {
    save_history_insts();
}

void shell::run() {
    this->history.push_back("new command");
}

int main() {
    shell shell;
    // while (true) {
    //     shell.run();
    // }
    shell.run();
}