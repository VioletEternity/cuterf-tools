#ifndef LIBNANOVNA_H
#define LIBNANOVNA_H

#include <complex>
#include <string>
#include <vector>

namespace libnanovna {

constexpr uint16_t NANOVNA_VID = 0x0483;
constexpr uint16_t NANOVNA_PID = 0x5740;

struct point 
{
    unsigned freq;
    std::complex<float> s11;
    std::complex<float> s21;
};

class nanovna_impl;

class nanovna 
{
private:
    nanovna_impl *m_i;

public:
    nanovna();
    ~nanovna();

    std::wstring path() const;

    std::string board_name() const;
    std::string firmware_info() const;
    std::string timestamp(); // host timestamp at the moment

    float edelay(); // in ps
    float s21offset(); // in dB

    bool is_open() const;
    bool open(const std::wstring &path = L"");
    void close();

    std::string capture_screenshot();

    std::vector<std::string> capture_header();
    std::vector<point> capture_data(unsigned ports);
    std::string capture_touchstone(unsigned ports);
};

};

#endif // NANOVNA_H
