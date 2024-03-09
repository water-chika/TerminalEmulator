#pragma once

#include <string>
#include <vector>


class terminal_sequence_lexer {
public:
    int lex_char(char c) {
        switch (state) {
            case 0:
            {
                if (c == '\x1b') {
                    state = 1;
                }
                else {
                    return c;
                }
            }
            break;
            case 1:
            {
                if (c == '[') {
                    state = 2;
                }
                else {
                    state = 0;
                }
            }
            break;
            case 2:
            if (isdigit(c)) {
                state = 2;
            }
            else if (c == ';') {
                state = 3;
            }
            else {
                state = 0;
            }
            break;
            case 3:
            if (isdigit(c)) {
                state = 3;
            }
            else {
                state = 0;
            }
            break;
        }
        return 0;
    }
    std::vector<char> lex(std::string_view str) {
        std::vector<char> res;
        for (auto c : str) {
            auto r = lex_char(c);
            if (r != 0) {
                res.push_back(r);
            }
        }
        return res;
    }
private:
    int state;
};