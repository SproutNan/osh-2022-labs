// IO
#include <iostream>
// std::string
#include <string>
// std::vector
#include <vector>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <sys/wait.h>

// trim from left
inline std::string &LeftTrim(std::string &s, const char *t = " \t\n\r\f\v") {
    auto begin = s.find_first_not_of(t);
    if (begin == s.npos) {
        s.clear();
    }
    else {
        auto size = s.size();
        s = s.substr(begin, size - begin);
    }
    return s;
}

// trim from right
inline std::string &RightTrim(std::string &s, const char *t = " \t\n\r\f\v") {
    auto end = s.find_last_not_of(t);
    if (end == s.npos) {
        s.clear();
    }
    else {
        s = s.substr(0, end + 1);
    }
    return s;
}

// trim from left & right
inline std::string &Trim(std::string &s, const char *t = " \t\n\r\f\v") {
    return LeftTrim(RightTrim(s, t), t);
}

std::string OmitSpace(std::string& s) {
    std::string ans;
    bool flag = false;
    int length = s.length();
    for (int i = 0; i < length; i++) {
        if (s[i] == ' ') {
            if (flag) {
                continue;
            }
            else {
                flag = true;
                ans += s[i];
            }
        }
        else {
            ans += s[i];
            flag = false;
        }
    }
    return ans;
}

std::string TrimOmit(std::string& s) {
    return OmitSpace(Trim(s));
}
// classic implementation of <cpp string split>
// additional feature: 
//   1. Trim the given line.
//   2. if delimiter is " ", omit additional delimiter. 
std::vector<std::string> split(std::string s, const std::string &delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    s = Trim(s);
    s = OmitSpace(s);
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        if (token.length()) {
            token = Trim(token);
            res.push_back(token);
        }
        s = s.substr(pos + delimiter.length());
    }
    if (s.length()) {
        s = Trim(s);
        res.push_back(s);
    }

    return res;
}