#include <stdexcept>
#include <initguid.h>
#include <windows.h>
#include <strsafe.h>
#include <Setupapi.h>
#include "serial.h"

namespace cuterf {

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

serial_port::serial_port() : hPort(INVALID_HANDLE_VALUE) 
{}

serial_port::~serial_port() 
{
    close();
}

bool serial_port::is_open() const
{
    return hPort != INVALID_HANDLE_VALUE;
}

bool serial_port::open(std::wstring path) 
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

void serial_port::close()
{
    CloseHandle(hPort);
    hPort = INVALID_HANDLE_VALUE;
}

void serial_port::write(std::string data)
{
    if (!WriteFile(hPort, &data[0], (DWORD)data.size(), NULL, NULL))
        throw std::runtime_error("WriteFile() failed");
}

void serial_port::read(std::string &data)
{
    if (!ReadFile(hPort, &data[0], (DWORD)data.size(), NULL, NULL))
        throw std::runtime_error("ReadFile() failed");
}

void serial_port::read_until(std::string expected, std::string *data)
{
    std::string buffer(1, '\0');
    while (ReadFile(hPort, &buffer[buffer.length() - 1], 1, NULL, NULL)) {
        if (buffer.size() >= expected.size() && buffer.substr(buffer.size() - expected.size(), expected.size()) == expected) {
            if (data != nullptr)
                *data = buffer.substr(0, buffer.size() - expected.size());
            return;
        }
        buffer += '\0';
    }
    throw std::runtime_error("ReadFile() failed");
}

}