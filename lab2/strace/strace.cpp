#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include "strace.h"

bool is_debug = false;
initializer init;

void MESS(std::string message) {
    std::cout << message << '\n';
}

void FATAL(std::string message) {
    MESS(message);
    exit(-1);
}

void TRACEE(char **argv) {
    if (is_debug) {
        std::cout << "TRACEE_PID: " << getpid() << '\n';
    }
    
    if (ptrace(PTRACE_TRACEME, NULL, NULL, NULL) < 0) {
        FATAL("error in PTRACE_TRACEME");
    }
    execvp(argv[0], argv);
}

void TRACEE(pid_t pid) {
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
        FATAL("error in PTRACE_ATTACH");
    }
}

void PRINT(struct user_regs_struct& regs, std::vector<uint32_t>& syscall_history, bool is_exit = false) {
    std::cout << init.get_name_by_number(regs.orig_rax);
    std::cout << "(";
    std::cout << static_cast<long int>(regs.rdi) << ", ";
    std::cout << static_cast<long int>(regs.rsi) << ", ";
    std::cout << static_cast<long int>(regs.rdx) << ", ";
    std::cout << static_cast<long int>(regs.r10) << ", ";
    std::cout << static_cast<long int>(regs.r8) << ", ";
    std::cout << static_cast<long int>(regs.r9);
    std::cout << ") = ";
    std::string return_prefix;
    auto syscall_ret = regs.rax;
    if (syscall_ret > 15) {
        return_prefix += "0x";
    }
    if (!is_exit) {
        std::cout << return_prefix << syscall_ret << '\n';
    }
    else {
        std::cout << "? \n";
    }
    syscall_history.push_back(regs.orig_rax);
}

void TRACER(int child_pid) {
    int child_status;
    if (is_debug) {
        std::cout << "TRACER_PID: " << getpid() << '\n';
    }

    wait(&child_status);

    auto syscall_state = PRE_SYSCALL;
    struct user_regs_struct regs;
    std::vector<uint32_t> syscall_history;

    if (ptrace(PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_EXITKILL) < 0) {
        FATAL("error set ptrace options");
    }


    while (true) {
        if (ptrace(PTRACE_SYSCALL, child_pid, 0, 0) < 0) {
            FATAL("error tracing syscalls");
        }
        wait(&child_status);

        if (WIFEXITED(child_status)) {
            PRINT(regs, syscall_history, true);
            std::cout << "+++ exited with " << child_status << " +++\n";
            break;
        }

        if (ptrace(PTRACE_GETREGS, child_pid, 0, &regs) < 0) {
            FATAL("error getting registers");
        }
        if (syscall_state == POST_SYSCALL) {
            PRINT(regs, syscall_history);
            syscall_state = PRE_SYSCALL;
        }
        else {
            syscall_state = POST_SYSCALL;
        }
    }
}

int main(int argc, char *argv[]) {
    // argv[0] = "./strace";
    // argv[1] = "true";
    // argc = 2;

    if (argc < 2) {
        FATAL("must have PROG [ARGS] or -p PID");
    }

    // parsing
    if (strcmp(argv[1], "-p") == 0) {
        if (argc < 3) {
            FATAL("must have PROG [ARGS] or -p PID");
        }
        pid_t pid;
        try {
            pid = atoi(argv[2]);
        }
        catch (...) {
            FATAL("invalid pid");
        }
        TRACEE(pid);
        TRACER(pid);
    }
    else {
        // if (strcmp(argv[1], "-d") == 0) {
        //     is_debug = true;
        // }
        pid_t pid = fork();
        switch (pid) {
        case -1:
            FATAL("error forking");
            break;
        case 0:
            TRACEE(argv + 1/* + is_debug ? 1 : 0*/);
            break;
        default:
            TRACER(pid);
            break;
        }
    }
    return 0;
}
