#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <initguid.h>
#include <windows.h>
#include <strsafe.h>
#include <Setupapi.h>
#include <png.h>

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
    }

    bool open(std::wstring uncPath) 
    {
        hPort = CreateFile(uncPath.c_str(), GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
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

    bool write(std::string data)
    {
        if (WriteFile(hPort, &data[0], (DWORD)data.size(), NULL, NULL))
            return true;
        return false;
    }

    bool read(std::string &data)
    {
        if (ReadFile(hPort, &data[0], (DWORD)data.size(), NULL, NULL))
            return true;
        return false;
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

struct screenshot 
{
    static constexpr size_t display_width = 480;
    static constexpr size_t display_height = 320;
    typedef uint16_t pixel;

    pixel data[display_width * display_height];

    std::vector<uint8_t> to_rgb24(int scale) const {
        static const uint8_t lut6to8[] = {
            0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  45,  49,  53,  57,  61,
            65,  69,  73,  77,  81,  85,  89,  93,  97,  101, 105, 109, 113, 117, 121, 125,
            130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190,
            194, 198, 202, 206, 210, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
        };
        std::vector<uint8_t> rgb24_data;
        for (size_t row = 0; row < display_height; row++) {
            for (int yrepeat = 0; yrepeat < scale; yrepeat++) {
                for (size_t column = 0; column < display_width; column++) {
                    pixel pixel = data[column + display_width * row];
                    for (int xrepeat = 0; xrepeat < scale; xrepeat++) {
                        rgb24_data.push_back(lut6to8[(pixel & 0xf800) >> 10]);
                        rgb24_data.push_back(lut6to8[(pixel & 0x07e0) >> 5]);
                        rgb24_data.push_back(lut6to8[(pixel & 0x001f) << 1]);
                    }
                }
            }
        }
        return rgb24_data;
    }
};

bool ReadScreenshotFromNanoVNA(std::wstring port_unc_path, screenshot &screenshot) 
{
    serial_port nano_vna;
    if (!nano_vna.open(port_unc_path))
        return false;

    if (!nano_vna.write("#sync#\r\n"))
        return false;
    if (!nano_vna.read_until("#sync#\r\n#sync#?\r\nch> "))
        return false;

    std::string info;
    if (!nano_vna.write("info\r\n"))
        return false;
    if (!nano_vna.read_until("\r\nch> ", &info))
        return false;
    
    std::string expected_info = "info\r\nBoard: NanoVNA-H 4\r\n";
    if (info.substr(0, expected_info.length()) != expected_info) {
        std::wcerr << L"Connected board type is not NanoVNA-H 4!" << std::endl;
        return false;
    }

    if (!nano_vna.write("capture\r\n"))
        return false;
    if (!nano_vna.read_until("capture\r\n"))
        return false;

    std::string display_data(sizeof(screenshot.data), '\0');
    if (!nano_vna.read(display_data))
        return false;
    if (!nano_vna.read_until("ch> "))
        return false;

    for (size_t i = 0; i < display_data.length(); i += 2)
        screenshot.data[i / 2] = (display_data[i] << 8) | (display_data[i + 1] & 0xff);
    return true;
}

bool SaveScreenshotToPNGFile(std::wstring path, const screenshot &screenshot, int scale)
{
    FILE *file = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    std::vector<uint8_t> rgb24_data;
    std::vector<png_bytep> rows;
    bool result = false;
    
    file = _wfopen(path.c_str(), L"wb");
    if (!file) 
        goto done;

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        goto done;

    info = png_create_info_struct(png);
    if (!info)
        goto done;

    if (setjmp(png_jmpbuf(png)))
        goto done;

    png_set_IHDR(png, info,
        screenshot.display_width * scale,
        screenshot.display_height * scale,
        8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    rgb24_data = screenshot.to_rgb24(scale);
    for (size_t row = 0; row < screenshot.display_height * scale; row++)
        rows.push_back(&rgb24_data[3 * screenshot.display_width * scale * row]);
    png_set_rows(png, info, &rows[0]);

    png_init_io(png, file);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, 0);
    result = true;

done:
    if (png != NULL)
        png_destroy_write_struct(&png, &info);
    if (file != NULL)
        fclose(file);
    return result;
}

int wmain(int argc, WCHAR** argv) 
{
    bool show_usage = false;
    int usage_status = EXIT_SUCCESS;
    std::wstring screenshot_path;
    int scale = 1;
    for (size_t argn = 1; argn < (size_t)argc; argn++) {
        if (!wcscmp(argv[argn], L"/?")) {
            show_usage = true;
            break;
        } else if (!wcsncmp(argv[argn], L"/scale:", 7) || !wcsncmp(argv[argn], L"/x", 2)) {
            WCHAR *szScale, *szScaleEnd;
            if (argv[argn][2] == 's')
                szScale = &argv[argn][7];
            else
                szScale = &argv[argn][2];
            scale = wcstol(szScale, &szScaleEnd, 10);
            if (*szScaleEnd != L'\0' || !(scale >= 1 && scale <= 4)) {
                std::wcerr << L"Scale should be 1 to 4 inclusive!" << std::endl;
                show_usage = true;
                usage_status = EXIT_FAILURE;
                break;
            }
        } else if (wcscmp(argv[argn], L"/") && screenshot_path.empty()) {
            screenshot_path = argv[argn];
        } else {
            std::wcerr << L"Unrecognized argument '" << argv[argn] << "'!" << std::endl;
            show_usage = true;
            usage_status = EXIT_FAILURE;
        }
    }
    if (show_usage) {
        std::wcerr << L"Usage: nanovna_capture.exe [options] [filename]" << std::endl;
        std::wcerr << L"Options:" << std::endl;
        std::wcerr << "\t/?\t\tShow program usage." << std::endl;
        std::wcerr << "\t/scale:N, /xN\tEnlarge image by factor of N (1 <= N <= 4)." << std::endl;
        return usage_status;
    }
    if (screenshot_path.empty()) {
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        wchar_t buffer[128];
        StringCchPrintf(buffer, sizeof(buffer), 
            L"NanoVNA_Screenshot_%02d%02d%02d_%02d%02d%02d.png",
            timeinfo->tm_year % 100, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        screenshot_path = std::wstring(buffer);
    }

    std::wstring port_unc_path;
    if (!FindUSBSerialPortByVIDPID(0x0483, 0x5740, port_unc_path)) {
        std::wcerr << L"Cannot find a connected NanoVNA!" << std::endl;
        return EXIT_FAILURE;
    }
    std::wcerr << "Found NanoVNA at '" << port_unc_path << L"'" << std::endl;

    screenshot screenshot;
    if (!ReadScreenshotFromNanoVNA(port_unc_path, screenshot)) {
        std::wcerr << L"Failed to read screenshot from NanoVNA!" << std::endl;
        return EXIT_FAILURE;
    }

    if (!SaveScreenshotToPNGFile(screenshot_path, screenshot, scale)) {
        std::wcerr << L"Failed to write screenshot to '" << screenshot_path << L"'!" << std::endl;
        return EXIT_FAILURE;
    }

    std::wcerr << L"Wrote screenshot to '" << screenshot_path << L"'" << std::endl;
    return EXIT_SUCCESS;
}
