#pragma once

template<class T, size_t Dim0_size, size_t Dim1_size, size_t Dim = 2>
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
        auto operator+(const int i) const {
            elem_ref res{ m_array, x,y };
            res.x += i;
            res.y += res.x / Dim0_size;
            res.x %= Dim0_size;
            return res;
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
    auto size() const {
        return Dim0_size * Dim1_size;
    }
    constexpr auto get_dim0_size() const {
        return Dim0_size;
    }
    constexpr auto get_dim1_size() const {
        return Dim1_size;
    }
    T& operator[](std::pair<int, int> index) {
        auto [x, y] = index;
        return m_data[y][x];
    }
private:
    std::array<std::array<T, Dim0_size>, Dim1_size > m_data;
};

template<class T, size_t Dim = 2>
class multidimention_vector {
    static_assert(Dim == 2);
public:
    multidimention_vector() :
        multidimention_vector(0, 0) {}
    multidimention_vector(size_t width, size_t height) :
        m_width{ width },
        m_stride{ width },
        m_height{ height },
        m_data(width*height)
    {}
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
            if (x + 1 == m_array.m_width) {
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
                x = m_array.m_width - 1;
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
        auto operator+(const int i) const {
            elem_ref res{ m_array, x,y };
            res.x += i;
            res.y += res.x / m_array.m_width;
            res.x %= m_array.m_width;
            return res;
        }
        auto operator-(const elem_ref& rhs) const {
            return (rhs.y - y) * m_array.m_width+ rhs.x - x;
        }
        bool operator==(elem_ref rhs) const {
            return x == rhs.x && y == rhs.y;
        }
        multidimention_vector& m_array;
        size_t x;
        size_t y;
    };
    using value_type = T;
    auto begin() {
        return elem_ref{ *this, 0, 0 };
    }
    auto end() {
        return elem_ref{ *this, 0, m_height };
    }
    auto size() const {
        return m_width * m_height;
    }
    constexpr auto get_dim0_size() const {
        return m_width;
    }
    constexpr auto get_dim1_size() const {
        return m_height;
    }
    constexpr auto get_width() const {
        return m_width;
    }
    constexpr auto get_height() const {
        return m_height;
    }
    size_t get_linear_index(std::pair<size_t, size_t> index) {
        auto [x, y] = index;
        assert(x < m_width && y < m_height);
        return y * m_width + x;
    }
    T& operator[](std::pair<size_t, size_t> index) {
        auto [x, y] = index;
        assert(x < m_width && y < m_height);
        return m_data[y*m_stride + x];
    }
private:
    std::vector<T> m_data;
    size_t m_width;
    size_t m_stride;
    size_t m_height;
};
