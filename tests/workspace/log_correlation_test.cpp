#include "modules/LogCorrelation.hpp"
#include <atomic>
int main() {
    using namespace lucid::modules;
    const std::vector<LogInput> inputs{{"a", "2024-01-01T01:00:01+0100 later\ninvalid\n2024-02-30T00:00:00Z bad\n"}, {"b", "[2024-01-01 00:00:00.123Z] first\r\n2024-01-01T00:00:01Z equal\n"}};
    auto r = correlateLogs(inputs);
    if (r.incomplete || r.unparseableLines != 2 || r.scannedLines != 5 || r.records.size() != 3 || r.records[0].source != "b" || r.records[1].source != "a" || r.records[2].source != "b" || r.records[0].timestampMillis + 877 != r.records[1].timestampMillis) return 1;
    r = correlateLogs(inputs, "later"); if (r.records.size() != 1 || r.records[0].line != 1 || r.unparseableLines != 2) return 2;
    r = correlateLogs(inputs, {}, 10000, 2); if (!r.incomplete || r.scannedLines != 2) return 3;
    r = correlateLogs(inputs, {}, 2); if (!r.incomplete || r.scannedLines != 0) return 4;
    r = correlateLogs(inputs, {}, 10000, 100, 1); if (!r.incomplete || r.records.size() != 1) return 5;
    const std::vector<LogInput> sys{{"sys", "Jan  1 01:00:00 host event\n  stack detail\n\torigin\ninvalid\n  orphan\nFeb 30 00:00:00 host bad\n"}, {"iso", "2024-01-01T00:00:00Z same\n"}};
    r = correlateLogs(sys); if (r.unparseableLines != 6 || r.records.size() != 1) return 6;
    LogCorrelationOptions options{2024, 60, true};
    r = correlateLogs(sys, "stack detail", 10000, 100, 100, options);
    if (r.records.size() != 1 || r.records[0].text.find("origin") == std::string::npos || r.unparseableLines != 3) return 7;
    r = correlateLogs(sys, {}, 10000, 100, 100, options);
    if (r.records.size() != 2 || r.records[0].timestampMillis != r.records[1].timestampMillis) return 8;
    options.syslogYear = 0; r = correlateLogs(sys, {}, 10000, 100, 100, options); if (r.unparseableLines != 6) return 9;
    const std::vector<LogInput> unordered{{"later", "2025-01-01T00:00:00Z late\n2024-01-01T00:00:00Z first\n"}, {"earlier", "2023-01-01T00:00:00Z earliest\n2024-01-01T00:00:00Z tied\n"}};
    r = correlateLogs(unordered, {}, 10000, 100, 2);
    if (!r.incomplete || r.records.size() != 2 || r.records[0].source != "earlier" || r.records[1].source != "later" || r.records[1].line != 2) return 10;
    const std::vector<LogInput> malformed{{"a", "[2024-01-01T00:00:00Z missing-close\n2024-01-01T00:00:00Z] stray-close\n[2024-01-01T00:00:00Z] valid\n[2024-01-01T00:00:00Z]compact\n"}};
    r = correlateLogs(malformed); if (r.unparseableLines != 2 || r.records.size() != 2 || r.records[0].line != 3 || r.records[1].line != 4) return 11;
    std::atomic_bool cancelled{true}; r = correlateLogs(inputs, {}, 10000, 100, 100, {}, &cancelled);
    if (!r.incomplete || !r.records.empty() || r.scannedLines != 0) return 12;
    const std::string longSource(4097, 'x');
    r = correlateLogs({{longSource, "2024-01-01T00:00:00Z ignored\n"}, {"safe", "2024-01-01T00:00:00Z kept\n"}});
    if (!r.incomplete || r.records.size() != 1 || r.records[0].source != "safe") return 13;
    r = correlateLogs(inputs, std::string(65537, 'x')); if (!r.incomplete || !r.records.empty()) return 14;
    const std::vector<LogInput> structured{{"api", R"({"timestamp":"2024-01-01T01:00:00+01:00","message":"request","trace":"secret"})" "\n" R"({"@timestamp":"2024-01-01T00:00:00.500Z","level":"error"})" "\n" R"({"time":"2024-01-01T00:00:00Z","timestamp":"2024-01-01T00:00:00Z"})" "\n" R"({"timestamp":"2024-01-01T00:00:00Z extra"})" "\n" R"({"time":1704067200})" "\n" R"({"timestamp":"2024-01-01T00:00:00"})" "\n"}, {"worker", "2024-01-01T00:00:00Z same\n"}};
    r=correlateLogs(structured); if(r.records.size()!=1 || r.unparseableLines!=6 || r.jsonLines!=0) return 15;
    LogCorrelationOptions jsonOptions{}; jsonOptions.enableJsonLines=true;
    r=correlateLogs(structured,{},10000,100,100,jsonOptions); if(r.incomplete || r.records.size()!=3 || r.jsonLines!=2 || r.unparseableLines!=4 || r.records[0].source!="api" || r.records[1].source!="worker" || r.records[2].line!=2 || r.records[0].timestampMillis!=r.records[1].timestampMillis || r.records[2].timestampMillis-r.records[1].timestampMillis!=500) return 16;
    r=correlateLogs(structured,"secret",10000,100,100,jsonOptions); if(r.records.size()!=1 || r.records[0].line!=1 || r.jsonLines!=2) return 17;
    const std::vector<LogInput> invalidJson{{"a", "2024-01-01T00:00:00Z first\n  {bad}\n  continuation\n" R"({"timestamp":"2024-01-01T00:00:00Z","timestamp":"2024-01-01T00:00:00Z"})" "\n"}};
    r=correlateLogs(invalidJson,{},10000,100,100,jsonOptions); if(r.records.size()!=1 || r.unparseableLines!=3 || r.jsonLines!=0 || r.records[0].text.find("bad")!=std::string::npos) return 18;
    std::string huge="{\"timestamp\":\"2024-01-01T00:00:00Z\",\"message\":\""+std::string(65536,'x')+"\"}"; r=correlateLogs({{"a",huge}}, {}, 100000,100,100,jsonOptions); if(r.records.size()!=0 || r.unparseableLines!=1) return 19;
    return 0;
}
