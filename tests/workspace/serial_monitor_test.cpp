#include "modules/SerialCapture.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#if !defined(_WIN32)
#if defined(__linux__)
#include <pty.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else
#include <util.h>
#endif
#include <unistd.h>
#endif
int main() {
    using namespace lucid::modules; SerialMonitorBuffer buffer(4); buffer.append("abcdef"); auto view=buffer.snapshot(); if(view.bytes!="cdef" || view.dropped!=2) return 1;
    SerialMonitorBuffer invalid(0); if(monitorSerial("/dev/null",9600,std::chrono::seconds(1),invalid).error.empty()) return 2;
    std::atomic_bool cancel{true}; if(!monitorSerial("/dev/null",9600,std::chrono::seconds(1),buffer,&cancel).cancelled) return 3;
#if !defined(_WIN32)
    int master=-1, slave=-1; char name[256]{}; if(openpty(&master,&slave,name,nullptr,nullptr)!=0) return 4;
    SerialMonitorBuffer stream(4); cancel=false; SerialCaptureResult result;
    std::thread thread([&]{result=monitorSerial(name,9600,std::chrono::seconds(2),stream,&cancel);});
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); if(write(master,"abcdef",6)!=6) {cancel=true;thread.join();close(master);close(slave);return 5;}
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(1); while(stream.snapshot().dropped<2 && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cancel=true; thread.join(); close(master); close(slave); view=stream.snapshot(); if(!result.cancelled || !result.error.empty() || view.bytes!="cdef" || view.dropped!=2) return 6;
#endif
    return 0;
}
