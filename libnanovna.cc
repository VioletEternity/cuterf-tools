#include <cstdint>
#include <string>
#include <stdexcept>
#include <initguid.h>
#include <windows.h>
#include <strsafe.h>
#include <Setupapi.h>

#include "libnanovna.h"

namespace libnanovna {

namespace {

bool FindUSBSerialPortByVIDPID(uint16_t VID, uint16_t PID, std::wstring &port_unc_path) 
{
    WCHAR szExpectedHardwareId[128];
    StringCchPrintf(szExpectedHardwareId, sizeof(szExpectedHardwareId),
        L"USB\\VID_%04x&PID_%04x", VID, PID);

    HDEVINFO hDeviceInfoSet = SetupDiGetClassDevs(NULL, L"USB", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDeviceInfoSet == INVALID_HANDLE_VALUE)
        return false;

    SP_DEVINFO_DATA DeviceInfoData = {};
    DWORD dwDeviceIndex = 0;
    DeviceInfoData.cbSize = sizeof(DeviceInfoData);
    while (SetupDiEnumDeviceInfo(hDeviceInfoSet, dwDeviceIndex++, &DeviceInfoData)) {
        WCHAR szHardwareId[128];
        DWORD dwHardwareIdSize = 0;
        if (SetupDiGetDeviceRegistryProperty(hDeviceInfoSet, &DeviceInfoData, SPDRP_HARDWAREID, NULL, (LPBYTE)szHardwareId, (DWORD)sizeof(szHardwareId) * sizeof(WCHAR), &dwHardwareIdSize)) {
            if (wcsncmp(szHardwareId, szExpectedHardwareId, wcslen(szExpectedHardwareId)))
                continue;

            HKEY hDeviceRegistryKey = SetupDiOpenDevRegKey(hDeviceInfoSet, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
            if (hDeviceRegistryKey == INVALID_HANDLE_VALUE)
                continue;

            DWORD dwRegValueType = 0;
            WCHAR szPortName[16];
            DWORD dwPortNameSize = sizeof(szPortName) * sizeof(WCHAR);
            LSTATUS result = RegQueryValueEx(hDeviceRegistryKey, L"PortName", NULL, &dwRegValueType, (LPBYTE) szPortName, &dwPortNameSize);
                RegCloseKey(hDeviceRegistryKey);

            if (result != ERROR_SUCCESS || dwRegValueType != REG_SZ)
                continue;
            if (wcsncmp(szPortName, L"COM", 3))
                continue;

            port_unc_path = std::wstring(L"\\\\.\\") + szPortName;
            SetupDiDestroyDeviceInfoList(hDeviceInfoSet);
            return true;
        }
    }

    SetupDiDestroyDeviceInfoList(hDeviceInfoSet);
    return false;
}

struct serial_port 
{
    HANDLE hPort = INVALID_HANDLE_VALUE;

    ~serial_port() 
    {
        close();
    }

    bool is_open() const
    {
        return hPort != INVALID_HANDLE_VALUE;
    }

    bool open(std::wstring path) 
    {
        hPort = CreateFile(path.c_str(), GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPort == INVALID_HANDLE_VALUE)
            return false;
        
        COMMTIMEOUTS CommTimeouts = {};
        CommTimeouts.ReadIntervalTimeout = 0;
        CommTimeouts.ReadTotalTimeoutMultiplier = 0;
        CommTimeouts.ReadTotalTimeoutConstant = 0;
        CommTimeouts.WriteTotalTimeoutMultiplier = 0;
        CommTimeouts.WriteTotalTimeoutConstant = 0;
        SetCommTimeouts(hPort, &CommTimeouts);

        return true;
    }

    void close()
    {
        CloseHandle(hPort);
        hPort = INVALID_HANDLE_VALUE;
    }

    void write(std::string data)
    {
        if (!WriteFile(hPort, &data[0], (DWORD)data.size(), NULL, NULL))
            throw std::runtime_error("WriteFile() failed");
    }

    void read(std::string &data)
    {
        if (!ReadFile(hPort, &data[0], (DWORD)data.size(), NULL, NULL))
            throw std::runtime_error("ReadFile() failed");
    }

    void read_until(std::string expected, std::string *data = nullptr)
    {
        std::string buffer(1, '\0');
        while (ReadFile(hPort, &buffer[buffer.length() - 1], 1, NULL, NULL)) {
            if (buffer.size() >= expected.size() && buffer.substr(buffer.size() - expected.size(), expected.size()) == expected) {
                if (data != nullptr)
                    *data = buffer;
                return;
            }
            buffer += '\0';
        }
        throw std::runtime_error("ReadFile() failed");
    }
};

}

class nanovna_impl 
{
public:
    std::wstring m_path;
    serial_port m_port;
    std::string m_board, m_version;

    void synchronize();
    std::string run(const std::string &command);

    void detect_board();
};

nanovna::nanovna() : m_i(new nanovna_impl) 
{}

nanovna::~nanovna() 
{
    delete m_i; 
}

std::wstring nanovna::path() const
{
    return m_i->m_path;
}

std::string nanovna::board() const
{
    return m_i->m_board;
}

std::string nanovna::version() const
{
    return m_i->m_version;
}

bool nanovna::is_open() const
{
    return m_i->m_port.is_open();
}

bool nanovna::open(const std::wstring &path)
{
    m_i->m_path = path;
    if (m_i->m_path.empty())
        if (!FindUSBSerialPortByVIDPID(NANOVNA_VID, NANOVNA_PID, m_i->m_path))
            return false;
    if (!m_i->m_port.open(m_i->m_path))
        return false;

    m_i->synchronize();
    m_i->detect_board();
    return true;
}

void nanovna::close()
{
    m_i->m_port.close();
    m_i->m_board.clear();
    m_i->m_version.clear();
}

void nanovna_impl::synchronize()
{
    m_port.write("#sync#\r\n");
    m_port.read_until("#sync#\r\n#sync#?\r\nch> ");
}

std::string nanovna_impl::run(const std::string &command)
{
    std::string result;
    m_port.write(command + "\r\n");
    m_port.read_until(command + "\r\n");
    m_port.read_until("ch> ", &result);
    return result;
}

void nanovna_impl::detect_board()
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

std::string nanovna::capture_screenshot()
{   
    m_i->m_port.write("capture\r\n");
    m_i->m_port.read_until("capture\r\n");

    std::string display_data(2 * 480 * 320, '\0'); // only one resolution supported at the moment
    m_i->m_port.read(display_data);

    std::string prompt(4, '\0');
    m_i->m_port.read(prompt);
    if (prompt != "ch> ")
        throw std::runtime_error("device returned screenshot of wrong size!");
    
    return display_data;
}

std::vector<point> nanovna::capture_data(unsigned ports)
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

}