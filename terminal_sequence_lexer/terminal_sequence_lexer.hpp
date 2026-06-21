#pragma once

#include <string>
#include <vector>

enum class lex_type : uint8_t{
    none,
    character,
    clear,
    new_line,
    return_,
    table,
    backspace,
    alarm,
};

struct lex_result {
    lex_type t;
    uint32_t value;
};

class terminal_sequence_lexer {
public:
    lex_result lex_char(char c) {
        switch (state) {
            case 0:
            if (c == '\x1b') {
                state = 1;
            }
            else if (c == '\n') {
                state = 0;
                return {lex_type::new_line, 0};
            }
            else if (c == '\r') {
                state = 0;
                return {lex_type::return_, 0};
            }
            else if (c == '\t') {
                state = 0;
                return {lex_type::table, 0};
            }
            else if (c == '\b') {
                state = 0;
                return {lex_type::backspace, 0};
            }
            else if (c == '\a') {
                state = 0;
                return {lex_type::alarm, 0};
            }
            else if ((c & 0b11110000) == 0b11100000) {
                state = 0b11100000;
                value = (c & 0b00001111) << 12;
            }
            else {
                state = 0;
                return {lex_type::character, c};
            }
            break;
            case 1:
            if (c == '[') {
                state = 2;
            }
            else {
                state = 0;
            }
            break;
            case 2:
            if (isdigit(c)) {
                state = 2;
            }
            else if (c == ';') {
                state = 3;
            }
            else if (c == 'H') {
                state = 4;
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
            case 4:
            if (c == '\E') {
                state = 5;
            }
            else {
                state = 0;
            }
            break;
            case 5:
            if (c == '[') {
                state = 6;
            }
            else {
                state = 0;
            }
            break;
            case 6:
            if (c == '2') {
                state = 7;
            }
            else {
                state = 0;
            }
            break;
            case 7:
            if (c == 'J') {
                state = 0;
                return {lex_type::clear, 0};
            }
            else {
                state = 0;
            }
            break;
            case 0b11100000:
            value |= (c & 0b00111111) << 6;
            state = 0b11100001;
            break;
            case 0b11100001:
            value |= (c & 0b00111111);
            state = 0;
            return {lex_type::character, value};
            break;
        }
        return {lex_type::none, 0};
    }
    std::vector<lex_result> lex(std::string_view str) {
        std::vector<lex_result> res;
        for (auto c : str) {
            auto r = lex_char(c);
            if (r.t != lex_type::none) {
                res.push_back(r);
            }
        }
        return res;
    }
private:
    int state;
    uint32_t value;
};
