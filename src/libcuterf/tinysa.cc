#include <iostream>
#include <iomanip>
#include "cuterf.h"
#include "serial.h"

namespace cuterf {

namespace tinysa {

class device_impl 
{
public:
    std::wstring m_path;
    serial_port m_port;
    bool m_is_ultra;
    std::string m_firmware_version, m_hardware_version;

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

bool device::is_ultra() const
{
    return m_i->m_is_ultra;
}

std::string device::hardware_version() const
{
    return m_i->m_hardware_version;
}

std::string device::firmware_version() const
{
    return m_i->m_firmware_version;
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
    m_i->m_is_ultra = false;
    m_i->m_firmware_version.clear();
    m_i->m_hardware_version.clear();
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
    std::string version = run("version");

    size_t firmware_ver_nl_pos = version.find("\r\n");
    if (version.substr(0, 8) == "tinySA4_") {
        m_is_ultra = true;
        m_firmware_version = version.substr(9, firmware_ver_nl_pos - 9);
    } else if (version.substr(0, 7) == "tinySA_") {
        m_is_ultra = false;
        m_firmware_version = version.substr(8, firmware_ver_nl_pos - 8);
    } else
        throw std::runtime_error("cannot parse device type from response to version!");

    size_t hardware_ver_pos = version.find("HW Version:");
    if (hardware_ver_pos == std::string::npos)
        throw std::runtime_error("cannot parse hardware version from response to version!");
    size_t hardware_ver_nl_pos = version.find("\r\n", hardware_ver_pos);
    if (hardware_ver_nl_pos == std::string::npos)
        throw std::runtime_error("cannot parse hardware version from response to version!");
    m_hardware_version = version.substr(hardware_ver_pos + 11, hardware_ver_nl_pos - hardware_ver_pos - 11);
}

std::string device::capture_screenshot(size_t &width, size_t &height)
{   
    if (is_ultra()) {
        width = 480;
        height = 320;
    } else {
        width = 320;
        height = 240;
    }

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

}

}