#include "modules/UnixCrashReport.hpp"
int main(){using lucid::modules::parseGdbBacktrace;
 auto r=parseGdbBacktrace("Program received signal SIGSEGV, Segmentation fault.\nThread 1 (Thread 0x123 (LWP 42)):\n#0  0x1234 in foo () at app.c:10\n#1  bar () at app.c:20\nThread 2 (Thread 0x456 (LWP 43)):\n#0  0xabcd in worker ()\n");
 if(!r.error.empty()||r.incomplete||r.signal!="SIGSEGV"||r.threads.size()!=2||r.threads[0].frames.size()!=2||r.threads[0].frames[0].address!=0x1234||r.threads[0].frames[1].address||r.threads[1].number!=2)return 1;
 r=parseGdbBacktrace("#0  0xabc in main ()\n");if(!r.error.empty()||r.threads.size()!=1||r.threads[0].number!=0)return 2;
 r=parseGdbBacktrace("Thread 1 (foo):\n#0 0x1 in a\nThread 2 (bar):\n#0 0x2 in b\n",4096,1,1);if(!r.incomplete||r.threads.size()!=1||r.threads[0].frames.size()!=1)return 3;
 r=parseGdbBacktrace("#0 0xfffffffffffffffff in bad\n#1 good\n");if(!r.incomplete||r.ignoredLines!=1||r.threads[0].frames.size()!=1)return 4;
 if(parseGdbBacktrace("hello").error.empty()||parseGdbBacktrace("#0 x",2).error.empty())return 5;
 r=parseGdbBacktrace("#0 text\n"+std::string(4097,'x')+"\n");if(!r.incomplete||r.ignoredLines!=1)return 6;
 r=parseGdbBacktrace("Thread 1 (ok):\n#0 0x1 in a\nThread malformed\n#1 0x2 in b\n");if(!r.incomplete||r.threads[0].frames.size()!=1)return 7;
 using lucid::modules::parseLldbBacktrace;
 r=parseLldbBacktrace("(lldb) thread backtrace all\n* thread #1, name = 'main', stop reason = EXC_BAD_ACCESS\n  * frame #0: 0x00001234 app`main + 4\n    frame #1: app`start\n  thread #2, name = 'worker'\n    frame #0: 0xabcd lib`worker\n");
 if(!r.error.empty()||r.incomplete||r.threads.size()!=2||r.threads[0].frames.size()!=2||r.threads[0].frames[0].address!=0x1234||r.threads[0].frames[1].address||r.threads[1].number!=2)return 8;
 r=parseLldbBacktrace("thread #1, name = 'a'\nframe #0: 0x1 a\nthread #1, name = 'duplicate'\nframe #0: 0x2 b\n");if(!r.incomplete||r.threads.size()!=1||r.threads[0].frames.size()!=1)return 9;
 r=parseLldbBacktrace("thread #1, name = 'a'\nframe #0: 0xfffffffffffffffff bad\nframe #1: good\n",4096,1,1);if(!r.incomplete||r.ignoredLines!=1||r.threads[0].frames.size()!=1)return 10;
 if(parseLldbBacktrace("frame #0: a").error.empty() || parseLldbBacktrace("thread #1, a",2).error.empty())return 11;
 return 0;}
