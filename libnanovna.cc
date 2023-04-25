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
        CloseHandle(hPort);
        hPort = INVALID_HANDLE_VALUE;
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

    void write(std::string data)
    {
        if (!WriteFile(hPort, &data[0], (DWORD)data.size(), NULL, NULL))
            throw std::runtime_error("WriteFile() failed");
    }

    bool read(std::string &data)
    {
        if (!ReadFile(hPort, &data[0], (DWORD)data.size(), NULL, NULL))
            throw std::runtime_error("ReadFile() failed");
    }

    bool read_until(std::string expected, std::string *data = nullptr)
    {
        std::string buffer(1, '\0');
        while (ReadFile(hPort, &buffer[buffer.length() - 1], 1, NULL, NULL)) {
            if (buffer.size() >= expected.size() && buffer.substr(buffer.size() - expected.size(), expected.size()) == expected) {
                if (data != nullptr)
                    *data = buffer;
                return true;
            }
            buffer += '\0';
        }
        return false;
    }
};

}

class nanovna_impl {
public:
    std::wstring m_path;
    serial_port m_port;

    void synchronize();
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

void nanovna_impl::synchronize()
{
    m_port.write("#sync#\r\n");
    m_port.read_until("#sync#\r\n#sync#?\r\nch> ");
}

void nanovna_impl::detect_board()
{   
    std::string info;
    m_port.write("info\r\n");
    m_port.read_until("\r\nch> ", &info);
    
    std::string expected_info = "info\r\nBoard: NanoVNA-H 4\r\n";
    if (info.substr(0, expected_info.length()) != expected_info)
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

}