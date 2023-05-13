#ifndef LIBCUTERF_CUTERF_H
#define LIBCUTERF_CUTERF_H

#include <complex>
#include <string>
#include <vector>

namespace cuterf {

// --- NanoVNA ---------------------------------------------------------------

namespace nanovna {

constexpr uint16_t VID = 0x0483;
constexpr uint16_t PID = 0x5740;

struct point 
{
    unsigned freq;
    std::complex<float> s11;
    std::complex<float> s21;
};

class device_impl;

class device 
{
private:
    device_impl *m_i;

public:
    device();
    ~device();

    bool is_open() const;
    std::wstring path() const;

    bool open(const std::wstring &path = L"");
    void close();

    std::string board_name() const;
    std::string firmware_info() const;
    std::string timestamp(); // host timestamp at the moment

    float edelay(); // in ps
    float s21offset(); // in dB

    std::string capture_screenshot(size_t &width, size_t &height);

    std::vector<std::string> capture_header();
    std::vector<point> capture_data(unsigned ports);
    std::string capture_touchstone(unsigned ports);
};

};

// --- TinySA ----------------------------------------------------------------

namespace tinysa {

constexpr uint16_t VID = 0x0483;
constexpr uint16_t PID = 0x5740;

class device_impl;

class device 
{
private:
    device_impl *m_i;

public:
    device();
    ~device();

    bool is_open() const;
    std::wstring path() const;

    bool open(const std::wstring &path = L"");
    void close();

    bool is_ultra() const;
    std::string hardware_version() const;
    std::string firmware_version() const;

    std::string capture_screenshot(size_t &width, size_t &height);
};

};

};

#endif // LIBCUTERF_CUTERF_H
