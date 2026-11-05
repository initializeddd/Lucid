#include "modules/AppleCrashReport.hpp"
int main(){using lucid::modules::parseAppleCrashReport;
 auto r=parseAppleCrashReport("Code Type: ARM-64 (Native)\nException Type: EXC_BAD_ACCESS (SIGSEGV)\n\nThread 0 Crashed:: Dispatch queue: com.apple.main-thread\n0   MyApp  0x0000000100123456 foo + 24\n1   libSystem.B.dylib 0x0000000180001234 start + 8\n\nThread 1:: Dispatch queue: worker\n0   libsystem 0x0000000180002222 wait + 4\n");
 if(!r.error.empty()||r.incomplete||r.architecture!="ARM-64 (Native)"||r.exceptionType!="EXC_BAD_ACCESS (SIGSEGV)"||r.threads.size()!=2||!r.threads[0].crashed||r.threads[0].frames.size()!=2||r.threads[0].frames[0].address!=0x100123456ULL||r.threads[1].number!=1)return 1;
 r=parseAppleCrashReport("Exception Type: EXC_CRASH\nThread 0 Crashed:\n0 x 0x1 main + 0\nThread 1:\n0 y 0x2 work + 0\n",4096,1,1);if(!r.error.empty()||!r.incomplete||r.threads.size()!=1||r.threads[0].frames.size()!=1)return 2;
 r=parseAppleCrashReport("Code Type: X86-64\nThread 0:\n0 x 0xfffffffffffffffff bad\n1 x 0x2 good\n");if(!r.error.empty()||!r.incomplete||r.ignoredLines!=1||r.threads[0].frames.size()!=1)return 3;
 r=parseAppleCrashReport("Code Type: X\nThread 0:\n0 x 0x1 a\nThread 1 bad:\n1 x 0x2 b\n");if(!r.incomplete||r.threads[0].frames.size()!=1)return 4;
 if(parseAppleCrashReport("Thread 0:\n0 x 0x1 x\n").error.empty()||parseAppleCrashReport("Code Type: X\nThread 0:\n",2).error.empty())return 5;
 r=parseAppleCrashReport("Code Type: X\nThread 0:\n"+std::string(4097,'x')+"\n0 x 0x2 x\n");if(!r.incomplete||r.threads[0].frames.size()!=0)return 6;
 return 0;}
