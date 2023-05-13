#ifndef LIBCUTERF_SERIAL_H
#define LIBCUTERF_SERIAL_H

#include <windows.h>
#include <cstdint>
#include <string>

namespace cuterf {

bool FindUSBSerialPortByVIDPID(uint16_t VID, uint16_t PID, std::wstring &port_unc_path);

struct serial_port 
{
    HANDLE hPort;

    serial_port();
    ~serial_port();
    bool is_open() const;

    bool open(std::wstring path);
    void close();

    void write(std::string data);
    void read(std::string &data);
    void read_until(std::string expected, std::string *data = nullptr);
};

}

#endif // LIBCUTERF_SERIAL_H