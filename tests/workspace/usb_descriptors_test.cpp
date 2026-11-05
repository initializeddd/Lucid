#include "modules/UsbDescriptors.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
int main(){namespace fs=std::filesystem; using namespace lucid::modules;
 ObservedUsbDevice parsed{1,2}; if (!validateUsbDescriptor(0x1234,0xabcd,parsed) || parsed.vendor!=0x1234 || parsed.product!=0xabcd) return 4;
 if (validateUsbDescriptor(0,1,parsed) || validateUsbDescriptor(-1,1,parsed) || validateUsbDescriptor(65536,1,parsed) || parsed.vendor!=0x1234) return 5;
 if (!parseWindowsUsbHardwareId("USB\\VID_1234&PID_aBcD&REV_0100",parsed) || parsed.vendor!=0x1234 || parsed.product!=0xabcd) return 6;
 if (parseWindowsUsbHardwareId("COM3",parsed) || parseWindowsUsbHardwareId("USB\\VID_1234&PID_abcdX",parsed) || parseWindowsUsbHardwareId("USB\\VID_0000&PID_1234",parsed)) return 7;
 auto dir=fs::temp_directory_path()/("lucid-usb-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())); fs::create_directories(dir/"devices"/"usb1"); fs::create_directories(dir/"bus");
 {std::ofstream(dir/"devices"/"usb1"/"idVendor",std::ios::binary)<<"1234\n"; std::ofstream(dir/"devices"/"usb1"/"idProduct",std::ios::binary)<<"abcd\n";}
 std::error_code ec; fs::create_directory_symlink(dir/"devices"/"usb1",dir/"bus"/"1-1",ec); if(ec){fs::remove_all(dir); return 0;}
 auto r=scanLinuxUsbDescriptors(dir/"bus",dir/"devices"); if(!r.error.empty() || r.incomplete || r.devices.size()!=1 || r.devices[0].ids.vendor!=0x1234 || r.devices[0].ids.product!=0xabcd) return 1;
 fs::create_directories(dir/"outside"); std::ofstream(dir/"outside"/"idVendor",std::ios::binary)<<"1111\n"; std::ofstream(dir/"outside"/"idProduct",std::ios::binary)<<"2222\n"; fs::create_directory_symlink(dir/"outside",dir/"bus"/"1-2",ec);
 r=scanLinuxUsbDescriptors(dir/"bus",dir/"devices"); if(!r.incomplete || r.devices.size()!=1) return 2;
 std::ofstream(dir/"devices"/"usb1"/"idVendor",std::ios::trunc | std::ios::binary)<<"123456789"; r=scanLinuxUsbDescriptors(dir/"bus",dir/"devices"); if(!r.incomplete || !r.devices.empty()) return 3;
 fs::remove_all(dir); return 0;}
