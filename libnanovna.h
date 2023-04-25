#ifndef LIBNANOVNA_H
#define LIBNANOVNA_H

#include <string>

namespace libnanovna {

constexpr uint16_t NANOVNA_VID = 0x0483;
constexpr uint16_t NANOVNA_PID = 0x5740;

class nanovna_impl;

class nanovna {
private:
    nanovna_impl *m_i;

public:
    nanovna();
    ~nanovna();

    std::wstring path() const;

    bool is_open() const;
    bool open(const std::wstring &path = L"");
    void close();

    std::string capture_screenshot();
};

};

#endif // NANOVNA_H
