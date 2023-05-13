#include <iostream>
#include <iomanip>
#include "cuterf.h"
#include "serial.h"

namespace cuterf {

namespace nanovna {

class device_impl 
{
public:
    std::wstring m_path;
    serial_port m_port;
    std::string m_board, m_version;

    void synchronize();
    std::string run(const std::string &command);

    void detect_board();
};

device::device() : m_i(new device_impl) 
{}

device::~device() 
{
    delete m_i; 
}

std::string device::timestamp()
{
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    std::stringstream ss;
    ss << 1900 + timeinfo->tm_year;
    ss << '-' << std::setw(2) << std::setfill('0') << timeinfo->tm_mon + 1;
    ss << '-' << std::setw(2) << std::setfill('0') << timeinfo->tm_mday;
    ss << ' ' << std::setw(2) << std::setfill('0') << timeinfo->tm_hour;
    ss << ':' << std::setw(2) << std::setfill('0') << timeinfo->tm_min;
    ss << ':' << std::setw(2) << std::setfill('0') << timeinfo->tm_sec;
    return ss.str();
}

bool device::is_open() const
{
    return m_i->m_port.is_open();
}

std::wstring device::path() const
{
    return m_i->m_path;
}

bool device::open(const std::wstring &path)
{
    m_i->m_path = path;
    if (m_i->m_path.empty())
        if (!FindUSBSerialPortByVIDPID(VID, PID, m_i->m_path))
            return false;
    if (!m_i->m_port.open(m_i->m_path))
        return false;

    m_i->synchronize();
    m_i->detect_board();
    return true;
}

void device::close()
{
    m_i->m_port.close();
    m_i->m_board.clear();
    m_i->m_version.clear();
}

void device_impl::synchronize()
{
    m_port.write("#sync#\r\n");
    m_port.read_until("#sync#\r\n#sync#?\r\nch> ");
}

std::string device_impl::run(const std::string &command)
{
    std::string result;
    m_port.write(command + "\r\n");
    m_port.read_until(command + "\r\n");
    m_port.read_until("ch> ", &result);
    return result;
}

void device_impl::detect_board()
{   
    std::string info = run("info");
    
    size_t board_pos = info.find("Board: ");
    if (board_pos == std::string::npos)
        throw std::runtime_error("cannot parse board from response to info!");
    size_t board_nl_pos = info.find("\r\n", board_pos);
    if (board_nl_pos == std::string::npos)
        throw std::runtime_error("cannot parse board from response to info!");
    m_board = info.substr(board_pos, board_nl_pos - board_pos).substr(7);

    size_t version_pos = info.find("Version: ", board_nl_pos);
    if (version_pos == std::string::npos)
        throw std::runtime_error("cannot parse version from response to info!");
    size_t version_nl_pos = info.find("\r\n", version_pos);
    if (version_nl_pos == std::string::npos)
        throw std::runtime_error("cannot parse version from response to info!");
    m_version = info.substr(version_pos, version_nl_pos - version_pos).substr(9);

    if (m_board != "NanoVNA-H 4")
        throw std::runtime_error("connected board type is not NanoVNA-H 4!");
}

std::string device::board_name() const
{
    return m_i->m_board;
}

std::string device::firmware_info() const
{
    return m_i->m_version;
}

float device::edelay()
{
    std::string buf = m_i->run("edelay");
    size_t pos;
    float edelay = std::stof(&buf[0], &pos);
    if (buf.substr(pos) != "\r\n")
        throw std::runtime_error("failed to parse response to edelay!");
    return edelay;
}

float device::s21offset()
{
    std::string buf = m_i->run("s21offset");
    size_t pos;
    float s21offset = std::stof(&buf[0], &pos);
    if (buf.substr(pos) != "\r\n")
        throw std::runtime_error("failed to parse response to s21offset!");
    return s21offset;
}

std::string device::capture_screenshot(size_t &width, size_t &height)
{   
    // only one resolution supported at the moment
    width = 480;
    height = 320;

    m_i->m_port.write("capture\r\n");
    m_i->m_port.read_until("capture\r\n");

    std::string display_data(2 * width * height, '\0');
    m_i->m_port.read(display_data);

    std::string prompt(4, '\0');
    m_i->m_port.read(prompt);
    if (prompt != "ch> ")
        throw std::runtime_error("device returned screenshot of wrong size!");
    
    return display_data;
}

std::vector<std::string> device::capture_header()
{
    std::vector<std::string> environment;
    environment.push_back("Board: " + board_name());
    environment.push_back("Firmware: " + firmware_info());
    environment.push_back("Timestamp: " + timestamp());
    float edelay = this->edelay();
    if (edelay != 0.0f)
        environment.push_back("E-delay: " + std::to_string(edelay) + " ps");
    float s21offset = this->s21offset();
    if (s21offset != 0.0f)
        environment.push_back("S21 offset: " + std::to_string(s21offset) + " dB");
    return environment;
}

std::vector<point> device::capture_data(unsigned ports)
{   
    if (!(ports == 1 || ports == 2))
        throw std::logic_error("can only capture data for 1 or 2 ports!");
 
    size_t pos;
    std::string buf;

    buf = m_i->run("sweep");
    unsigned start = std::stoi(buf, &pos);
    if (pos == 0 || buf[pos] != ' ')
        throw std::runtime_error("unrecognized format of response to sweep!");

    buf = buf.substr(pos);
    unsigned stop = std::stoi(buf, &pos);
    if (pos == 0 || buf[pos] != ' ')
        throw std::runtime_error("failed to parse response to sweep!");

    buf = buf.substr(pos);
    unsigned points = std::stoi(buf, &pos);
    if (pos == 0 || buf.substr(pos, 2) != "\r\n")
        throw std::runtime_error("failed to parse response to sweep!");
    
    // see set_frequencies() in firmware
    unsigned f_points, f_delta, f_error;
    f_points = points - 1;
    f_delta  = (stop - start) / f_points;
    f_error  = (stop - start) % f_points;

    // see getFrequency() in firmware
    std::vector<point> data(points);
    for (unsigned idx = 0; idx < points; idx++)
        data[idx].freq = start + f_delta * idx + (f_points / 2 + f_error * idx) / f_points;

    for (unsigned port = 1; port <= ports; port++) {
        buf = m_i->run("data " + std::to_string(port - 1));
        for (unsigned idx = 0; idx < points; idx++) {
            float re = std::stof(buf, &pos);
            if (pos == 0 || buf[pos] != ' ')
                throw std::runtime_error("failed to parse response to data!");
            
            buf = buf.substr(pos);
            float im = std::stof(buf, &pos);
            if (pos == 0 || buf.substr(pos, 2) != "\r\n")
                throw std::runtime_error("failed to parse response to data!");
            
            if (port == 1)
                data[idx].s11 = std::complex<float>(re, im);
            if (port == 2)
                data[idx].s21 = std::complex<float>(re, im);
            
            buf = buf.substr(pos);
        }
    }

    return data;
}

std::string device::capture_touchstone(unsigned ports)
{
    auto header = capture_header();
    auto data = capture_data(ports);

    std::stringstream ss;
    for (auto &line : header)
        ss << "! " << line << '\n';
    ss << "# HZ S RI R 50\n";
    for (auto &point : data) {
        std::vector<float> sxy;
        if (ports == 1 || ports == 2) {
            sxy.push_back(point.s11.real());
            sxy.push_back(point.s11.imag());
        }
        if (ports == 2) {
            sxy.push_back(point.s21.real());
            sxy.push_back(point.s21.imag());
            sxy.push_back(0.0f); // s12.re
            sxy.push_back(0.0f); // s12.im
            sxy.push_back(0.0f); // s22.re
            sxy.push_back(0.0f); // s22.im
        }
        ss << std::setw(10) << point.freq;
        for (auto s : sxy)
            ss << ' ' << std::fixed << std::setw(12) << std::showpos << std::setprecision(9) << s;
        ss << '\n';
    }
    return ss.str();
}

}

}