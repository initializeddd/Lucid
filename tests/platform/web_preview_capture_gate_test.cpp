#include "platform/browser/WebPreviewCaptureGate.hpp"
#include <chrono>
using lucid::platform::WebPreviewCaptureGate;
int main() {
    using namespace std::chrono_literals; WebPreviewCaptureGate gate; const auto now=WebPreviewCaptureGate::Clock::time_point{};
    gate.navigationCompleted(1,now); if (gate.takeReady(now+1s)) return 1;
    gate.navigationStarted(1); gate.navigationCompleted(1,now); if (gate.takeReady(now+99ms) || gate.takeReady(now+100ms)!=1 || gate.takeReady(now+1s)) return 2;
    gate.navigationCompleted(1,now); gate.navigationStarted(2); if (gate.isCurrentNavigation(1) || gate.takeReady(now+1s)) return 3;
    gate.navigationCompleted(1,now); if (gate.takeReady(now+1s)) return 4;
    gate.navigationCompleted(2,now); const auto prior=gate.takeReady(now+100ms); if (prior!=2 || !gate.isCurrentNavigation(2) || !gate.isCurrentCapture(*prior)) return 5;
    gate.navigationStarted(2); if (gate.isCurrentCapture(*prior)) return 7;
    gate.navigationStarted(3); gate.navigationCompleted(3,now); gate.cancel(); if (gate.isCurrentNavigation(3) || gate.takeReady(now+1s)) return 6;
    return 0;
}
