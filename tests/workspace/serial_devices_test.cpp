#include "modules/SerialDevices.hpp"
#include "modules/SerialCapture.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#if defined(__linux__)
#include <pty.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else
#include <util.h>
#endif
#endif
using namespace lucid::modules;
int main() {
    namespace fs = std::filesystem;
    bool ok = isSerialDeviceName(SerialDevicePlatform::Linux, "ttyUSB0") && isSerialDeviceName(SerialDevicePlatform::Linux, "ttyACM123") && !isSerialDeviceName(SerialDevicePlatform::Linux, "ttyUSBx") && !isSerialDeviceName(SerialDevicePlatform::Linux, "ttyUSB0extra");
    ok = ok && isSerialDeviceName(SerialDevicePlatform::MacOS, "cu.usbmodem123") && !isSerialDeviceName(SerialDevicePlatform::MacOS, "tty.usbmodem123") && !isSerialDeviceName(SerialDevicePlatform::MacOS, "cu.usbmodem");
    ok = ok && isSerialDeviceName(SerialDevicePlatform::Windows, "COM12") && !isSerialDeviceName(SerialDevicePlatform::Windows, "COM0") && !isSerialDeviceName(SerialDevicePlatform::Windows, "COM1x");
    auto root = fs::temp_directory_path() / ("lucid-serial-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "ttyUSB4"); std::ofstream(root / "ttyUSB2"); std::ofstream(root / "ttyACM0"); std::ofstream(root / "ordinary");
    auto scan = scanSerialDeviceDirectory(SerialDevicePlatform::Linux, root);
    ok = ok && scan.error.empty() && !scan.incomplete && scan.devices.size() == 2 && scan.devices[0].name == "ttyACM0" && scan.devices[1].name == "ttyUSB2";
    auto missing = scanSerialDeviceDirectory(SerialDevicePlatform::Linux, root / "missing"); ok = ok && !missing.error.empty();
    std::atomic_bool cancel{true};
    auto cancelled = captureSerial(root / "ttyUSB2", 115200, std::chrono::milliseconds(100), 1024, &cancel);
    ok = ok && cancelled.cancelled && cancelled.error.empty();
    ok = ok && !captureSerial(root / "ttyUSB2", 123, std::chrono::milliseconds(100)).error.empty();
    ok = ok && !captureSerial(root / "ttyUSB2", 9600, std::chrono::milliseconds(0)).error.empty();
    ok = ok && !captureSerial(root / "ttyUSB2", 9600, std::chrono::seconds(61)).error.empty();
    ok = ok && !captureSerial(root / "ttyUSB2", 9600, std::chrono::milliseconds(1), 0).error.empty();
    ok = ok && !captureSerial(root / "ttyUSB2", 9600, std::chrono::milliseconds(100)).error.empty();
#ifndef _WIN32
    int master = -1, slave = -1; char name[1024]{};
    if (openpty(&master, &slave, name, nullptr, nullptr) != 0) ok = false;
    else {
        termios before{}, after{}; ok = ok && tcgetattr(slave, &before) == 0;
        SerialCaptureResult capture;
        std::thread reader([&] { capture = captureSerial(name, 115200, std::chrono::milliseconds(500), 5); });
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        const auto written = write(master, "hello world", 11);
        reader.join();
        ok = ok && written == 11 && capture.error.empty() && capture.bytes == "hello" && capture.truncated && !capture.restoreFailed;
        const tcflag_t localModes = ECHO | ICANON | ISIG | IEXTEN;
        ok = ok && tcgetattr(slave, &after) == 0 && before.c_iflag == after.c_iflag && before.c_oflag == after.c_oflag && before.c_cflag == after.c_cflag && (before.c_lflag & localModes) == (after.c_lflag & localModes) && cfgetispeed(&before) == cfgetispeed(&after) && cfgetospeed(&before) == cfgetospeed(&after);
#ifdef O_NOFOLLOW
        std::error_code symlinkError;
        fs::create_symlink(name, root / "serial-link", symlinkError);
        if (!symlinkError) ok = ok && !captureSerial(root / "serial-link", 9600, std::chrono::milliseconds(10)).error.empty();
#endif
        cancel.store(false);
        std::thread interrupted([&] { capture = captureSerial(name, 9600, std::chrono::seconds(2), 1024, &cancel); });
        std::this_thread::sleep_for(std::chrono::milliseconds(120)); cancel.store(true); interrupted.join();
        ok = ok && capture.cancelled && capture.error.empty() && !capture.restoreFailed;
        close(master); close(slave);
    }
#endif
    fs::remove_all(root); return ok ? 0 : 1;
}
