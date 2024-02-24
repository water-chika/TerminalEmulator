#pragma once

template<class T, int Dim0_size, int Dim1_size, int Dim = 2>
class multidimention_array {
    static_assert(Dim == 2);
public:
    struct elem_ref {
        using difference_type = int;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::contiguous_iterator_tag;

        auto& operator=(const elem_ref& rhs) {
            m_array = rhs.m_array;
            x = rhs.x;
            y = rhs.y;
            return *this;
        }
        auto& operator++() {
            if (x + 1 == Dim0_size) {
                x = 0;
                ++y;
            }
            else {
                ++x;
            }
            return *this;
        }
        auto operator++(int) {
            auto ret = *this;
            ++*this;
            return ret;
        }
        auto& operator--() {
            if (x == 0) {
                x = Dim0_size - 1;
                --y;
            }
            else {
                --x;
            }
            return *this;
        }
        auto operator--(int) {
            auto ret = *this;
            --*this;
            return ret;
        }
        auto& operator*() {
            return m_array[std::pair{ x, y }];
        }
        auto operator-(const elem_ref& rhs) const{
            return (rhs.y - y) * Dim1_size + rhs.x - x;
        }
        bool operator==(elem_ref rhs) const {
            return x == rhs.x && y == rhs.y;
        }
        multidimention_array& m_array;
        int x;
        int y;
    };
    using value_type = T;
    auto begin() {
        return elem_ref{ *this, 0, 0 };
    }
    auto end() {
        return elem_ref{ *this, 0, Dim1_size };
    }
    auto size() {
        return Dim0_size * Dim1_size;
    }
    T& operator[](std::pair<int, int> index) {
        auto [x, y] = index;
        return m_data[y][x];
    }
private:
    std::array<std::array<T, Dim0_size>, Dim1_size > m_data;
};
