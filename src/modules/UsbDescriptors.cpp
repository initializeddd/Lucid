#include "modules/UsbDescriptors.hpp"
#include <algorithm>
#include <fstream>
#include <system_error>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <usbiodef.h>
#endif
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#endif
namespace lucid::modules {
namespace fs=std::filesystem;
namespace {
bool within(const fs::path& root,const fs::path& path) { auto a=root.begin(), b=path.begin(); for (;a!=root.end();++a,++b) if(b==path.end() || *a!=*b) return false; return true; }
bool idFile(const fs::path& path, const fs::path& root, std::uint16_t& result) {
    std::error_code ec; auto canonical=fs::canonical(path,ec); if(ec || !within(root,canonical) || !fs::is_regular_file(canonical,ec) || ec) return false;
    std::ifstream input(canonical,std::ios::binary); if(!input) return false; char buffer[8]{}; input.read(buffer,sizeof(buffer)); auto count=input.gcount();
    if(input.bad() || count<4 || count>5 || (count==5 && buffer[4]!='\n')) return false;
    unsigned n=0; for(int i=0;i<4;++i){char c=buffer[i];unsigned v=0; if(c>='0'&&c<='9') v=c-'0'; else if(c>='a'&&c<='f') v=c-'a'+10; else if(c>='A'&&c<='F') v=c-'A'+10; else return false; n=n*16+v;} if(!n) return false; result=static_cast<std::uint16_t>(n); return true;
}
}
UsbDescriptorScan scanLinuxUsbDescriptors(const fs::path& busDirectory,const fs::path& deviceRoot) {
    UsbDescriptorScan out; std::error_code ec; auto root=fs::canonical(deviceRoot,ec); if(ec || !fs::is_directory(root,ec) || ec) {out.error="USB descriptor root unavailable";return out;}
    fs::directory_iterator it(busDirectory,fs::directory_options::skip_permission_denied,ec),end; if(ec){out.error="USB descriptor directory unavailable";return out;}
    std::size_t seen=0;
    for(;it!=end;it.increment(ec)) {
        if(ec) {out.incomplete=true;out.error="USB descriptor directory read failed";break;}
        if(++seen>4096){out.incomplete=true;break;}
        auto name=it->path().filename().string();
        // Interface entries contain a colon and do not describe a USB device.
        if(name.empty() || name.size()>128 || name.find(':')!=std::string::npos) continue;
        if(!std::all_of(name.begin(),name.end(),[](unsigned char c){return (c>='0'&&c<='9') || (c>='a'&&c<='z') || c=='-' || c=='.';})) continue;
        auto dir=fs::canonical(it->path(),ec); if(ec || !within(root,dir) || !fs::is_directory(dir,ec) || ec) {ec.clear();out.incomplete=true;continue;}
        ObservedUsbDevice ids; if(!idFile(dir/"idVendor",root,ids.vendor) || !idFile(dir/"idProduct",root,ids.product)) {out.incomplete=true;continue;}
        if(out.devices.size()>=1024){out.incomplete=true;break;} out.devices.push_back({std::move(name),ids});
    }
    if(ec){out.incomplete=true;out.error="USB descriptor directory read failed";}
    std::sort(out.devices.begin(),out.devices.end(),[](const auto& a,const auto& b){return a.location<b.location;}); return out;
}
bool validateUsbDescriptor(std::int64_t vendor, std::int64_t product, ObservedUsbDevice& output) {
    if (vendor<=0 || vendor>65535 || product<=0 || product>65535) return false;
    output={static_cast<std::uint16_t>(vendor),static_cast<std::uint16_t>(product)}; return true;
}
bool parseWindowsUsbHardwareId(std::string_view id, ObservedUsbDevice& output) {
    // Hardware IDs (not instance IDs) begin with USB\\VID_XXXX&PID_XXXX.
    if (id.size()<21 || id.size()>4096) return false;
    const auto upper=[](char c){return c>='a' && c<='z' ? static_cast<char>(c-'a'+'A') : c;};
    constexpr std::string_view prefix="USB\\VID_", middle="&PID_";
    for (std::size_t i=0;i<prefix.size();++i) if(upper(id[i])!=prefix[i]) return false;
    for (std::size_t i=0;i<middle.size();++i) if(upper(id[12+i])!=middle[i]) return false;
    if (id.size()>21 && id[21]!='&') return false;
    auto hex=[&](std::size_t p,std::int64_t& n) { n=0; for(unsigned i=0;i<4;++i) {auto c=upper(id[p+i]); unsigned v; if(c>='0'&&c<='9') v=c-'0'; else if(c>='A'&&c<='F') v=c-'A'+10; else return false; n=n*16+v;} return true; };
    std::int64_t vendor=0,product=0; return hex(8,vendor) && hex(17,product) && validateUsbDescriptor(vendor,product,output);
}
UsbDescriptorScan discoverUsbDescriptors() {
#if defined(_WIN32)
    UsbDescriptorScan out;
    const GUID usbDevice = {0xA5DCBF10,0x6530,0x11D2,{0x90,0x1F,0x00,0xC0,0x4F,0xB9,0x51,0xED}};
    HDEVINFO list=SetupDiGetClassDevsW(&usbDevice,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (list==INVALID_HANDLE_VALUE) {out.error="Could not enumerate USB descriptors";return out;}
    DWORD index=0;
    for (;index<4096;++index) {
        SP_DEVINFO_DATA info{}; info.cbSize=sizeof(info);
        if (!SetupDiEnumDeviceInfo(list,index,&info)) {if(GetLastError()!=ERROR_NO_MORE_ITEMS) {out.error="USB descriptor enumeration failed";out.incomplete=true;} break;}
        DWORD type=0,needed=0; std::vector<wchar_t> values(4096);
        if (!SetupDiGetDeviceRegistryPropertyW(list,&info,SPDRP_HARDWAREID,&type,reinterpret_cast<PBYTE>(values.data()),static_cast<DWORD>(values.size()*sizeof(wchar_t)),&needed) || type!=REG_MULTI_SZ || needed<2*sizeof(wchar_t) || needed>values.size()*sizeof(wchar_t) || needed%sizeof(wchar_t)) {out.incomplete=true;continue;}
        const auto length=needed/sizeof(wchar_t); if (values[length-1]!=0 || values[length-2]!=0) {out.incomplete=true;continue;}
        bool found=false, malformed=false; ObservedUsbDevice ids;
        for (std::size_t p=0;p<length && values[p];) {
            std::string id; while(p<length && values[p]) {if(values[p]>127) malformed=true; else id.push_back(static_cast<char>(values[p])); ++p;} if(p==length){malformed=true;break;} ++p;
            ObservedUsbDevice candidate; if(parseWindowsUsbHardwareId(id,candidate)) {if(found && (candidate.vendor!=ids.vendor || candidate.product!=ids.product)) {malformed=true;break;} ids=candidate;found=true;}
        }
        if(!found || malformed) {out.incomplete=true;continue;}
        if(out.devices.size()>=1024) {out.incomplete=true;break;}
        out.devices.push_back({"Windows USB entry " + std::to_string(index+1),ids});
    }
    if(index==4096) out.incomplete=true;
    SetupDiDestroyDeviceInfoList(list); return out;
#elif defined(__APPLE__)
    UsbDescriptorScan out; auto matching=IOServiceMatching("IOUSBHostDevice");
    if (!matching) {out.error="Could not prepare USB descriptor discovery";return out;}
    io_iterator_t iterator=IO_OBJECT_NULL; const auto status=IOServiceGetMatchingServices(MACH_PORT_NULL,matching,&iterator);
    // IOServiceGetMatchingServices consumes the matching dictionary.
    if (status!=KERN_SUCCESS) {out.error="Could not enumerate USB descriptors";return out;}
    std::size_t seen=0;
    while (auto service=IOIteratorNext(iterator)) {
        if (++seen>4096) {out.incomplete=true;IOObjectRelease(service);break;}
        auto vendor=IORegistryEntryCreateCFProperty(service,CFSTR("idVendor"),kCFAllocatorDefault,0);
        auto product=IORegistryEntryCreateCFProperty(service,CFSTR("idProduct"),kCFAllocatorDefault,0);
        std::int64_t v=0,p=0; bool valid=vendor && product && CFGetTypeID(vendor)==CFNumberGetTypeID() && CFGetTypeID(product)==CFNumberGetTypeID() && CFNumberGetValue(static_cast<CFNumberRef>(vendor),kCFNumberSInt64Type,&v) && CFNumberGetValue(static_cast<CFNumberRef>(product),kCFNumberSInt64Type,&p);
        if (vendor) CFRelease(vendor); if (product) CFRelease(product);
        std::uint64_t registryId=0; valid=valid && IORegistryEntryGetRegistryEntryID(service,&registryId)==KERN_SUCCESS;
        IOObjectRelease(service); ObservedUsbDevice ids;
        if (!valid || !validateUsbDescriptor(v,p,ids)) {out.incomplete=true;continue;}
        if (out.devices.size()>=1024) {out.incomplete=true;break;}
        out.devices.push_back({"IOKit entry " + std::to_string(registryId),ids});
    }
    if (!IOIteratorIsValid(iterator)) out.incomplete=true;
    IOObjectRelease(iterator);
    std::sort(out.devices.begin(),out.devices.end(),[](const auto& a,const auto& b){return a.location<b.location;}); return out;
#elif defined(__linux__)
    return scanLinuxUsbDescriptors("/sys/bus/usb/devices","/sys/devices");
#else
    UsbDescriptorScan result; result.error="USB descriptor enumeration is not supported on this platform"; return result;
#endif
}
}
