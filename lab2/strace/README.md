# Lab2 STRACE部分

## 如何编译和运行

    g++ strace.cpp -o strace
    // 跟踪一个命令
    ./strace cmd
    // 跟踪某个pid
    ./strace pid

## 实现的必做部分

 - 能够追踪 syscall

## 实现的选做部分

 - 系统调用可以显示名字而不是整型数

