# Lab2 SHELL部分

## 如何编译和运行

    g++ shell.cpp -o shell
    ./shell
    
## 实现的必做部分

 - 支持多管道命令
 - 支持`>`, `>>`, `<`重定向
 - 支持history以及!, !n
 
## 实现的选做部分

 - 支持`;`分隔的多命令
 - 重定向支持`fd>`, `fd<`, `fd1>&fd2`语法
 - 支持Ctrl-C处理
 - 支持`echo ~root`,  `echo 含有$HOME的字符串`, `echo 含有$SHELL的字符串`, `echo 一般字符串`
 - 支持`A=1 B=3 env`之类设置环境变量的语法

