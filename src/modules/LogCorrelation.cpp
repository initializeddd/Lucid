#include "modules/LogCorrelation.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <chrono>

namespace lucid::modules {
namespace {
bool digits(std::string_view s, std::size_t p, std::size_t n, int& value) {
    if (p > s.size() || n > s.size() - p) return false;
    value = 0; for (std::size_t i = 0; i < n; ++i) { char c = s[p+i]; if (c < '0' || c > '9') return false; value = value * 10 + c - '0'; } return true;
}
bool timestamp(std::string_view s, bool bracketed, std::int64_t& result, bool exact = false) {
    int y, mo, d, h, m, sec;
    if (s.size() < 20 || !digits(s,0,4,y) || s[4]!='-' || !digits(s,5,2,mo) || s[7]!='-' || !digits(s,8,2,d) || (s[10]!='T' && s[10]!=' ') || !digits(s,11,2,h) || s[13]!=':' || !digits(s,14,2,m) || s[16]!=':' || !digits(s,17,2,sec)) return false;
    using namespace std::chrono;
    const year_month_day date{year{y}, month{static_cast<unsigned>(mo)}, day{static_cast<unsigned>(d)}};
    if (!date.ok() || h > 23 || m > 59 || sec > 59) return false;
    std::size_t p = 19; int millis = 0;
    if (p < s.size() && (s[p] == '.' || s[p] == ',')) {
        ++p; const auto start = p; int factor = 100;
        while (p < s.size() && s[p] >= '0' && s[p] <= '9') { if (factor) { millis += (s[p]-'0') * factor; factor /= 10; } ++p; }
        if (p == start) return false;
    }
    int offset = 0;
    if (p < s.size() && (s[p] == 'Z' || s[p] == 'z')) ++p;
    else if (p < s.size() && (s[p] == '+' || s[p] == '-')) {
        int sign = s[p++] == '+' ? 1 : -1, oh = 0, om = 0;
        if (!digits(s,p,2,oh)) return false;
        p += 2;
        if (p < s.size() && s[p] == ':') ++p;
        if (!digits(s,p,2,om) || oh > 23 || om > 59) return false;
        p += 2; offset = sign * (oh * 60 + om);
    } else return false;
    if (exact && p != s.size()) return false;
    if (bracketed) { if (p >= s.size() || s[p] != ']') return false; }
    else if (p < s.size() && s[p] != ' ' && s[p] != '\t') return false;
    result = duration_cast<milliseconds>(sys_days{date}.time_since_epoch()).count() + ((h * 60 + m - offset) * 60LL + sec) * 1000 + millis;
    return true;
}
bool jsonTimestamp(std::string_view line, std::int64_t& result) {
    if(line.size()>65536) return false;
    editor::JsonError error; auto root=editor::parseJson(line,error);
    if(!root || root->type!=editor::JsonType::Object) return false;
    const editor::JsonValue* found=nullptr;
    for(const auto& child:root->children) {
        if(child.key=="timestamp" || child.key=="@timestamp" || child.key=="time") {
            if(found) return false;
            found=&child;
        }
    }
    return found && found->type==editor::JsonType::String && timestamp(found->stringValue,false,result,true);
}

bool syslogTimestamp(std::string_view s, const LogCorrelationOptions& options, std::int64_t& result) {
    if (!options.enableSyslog || options.syslogYear < 1 || options.syslogYear > 9999 || options.syslogUtcOffsetMinutes < -1439 || options.syslogUtcOffsetMinutes > 1439 || s.size() < 15) return false;
    constexpr std::string_view months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    unsigned month = 0; for (unsigned i = 0; i < 12; ++i) if (s.substr(0,3) == months[i]) { month = i + 1; break; }
    int day = 0, h = 0, m = 0, sec = 0;
    if (!month || s[3] != ' ' || (s[4] != ' ' && (s[4] < '0' || s[4] > '9')) || !digits(s,5,1,day) || s[6] != ' ' || !digits(s,7,2,h) || s[9] != ':' || !digits(s,10,2,m) || s[12] != ':' || !digits(s,13,2,sec)) return false;
    if (s[4] != ' ') day += (s[4] - '0') * 10;
    if (s.size() > 15 && s[15] != ' ' && s[15] != '\t') return false;
    using namespace std::chrono; const year_month_day date{year{options.syslogYear}, std::chrono::month{month}, std::chrono::day{static_cast<unsigned>(day)}};
    if (!date.ok() || h > 23 || m > 59 || sec > 59) return false;
    result = duration_cast<milliseconds>(sys_days{date}.time_since_epoch()).count() + ((h * 60 + m - options.syslogUtcOffsetMinutes) * 60LL + sec) * 1000;
    return true;
}

// RFC 5424 records carry their own timezone and do not need the assumptions
// required for traditional BSD syslog. Only inspect the header, never execute
// or interpret message or structured-data content.
bool rfc5424Timestamp(std::string_view s, std::int64_t& result) {
    if(s.size()<6 || s.front()!='<') return false;
    const auto close=s.find('>');
    if(close==std::string_view::npos || close<2 || close>4 || (close>2 && s[1]=='0')) return false;
    int priority=0; if(!digits(s,1,close-1,priority) || priority>191) return false;
    s.remove_prefix(close+1);
    if(!s.starts_with("1 ")) return false;
    s.remove_prefix(2);
    const auto end=s.find(' '); if(end==std::string_view::npos || end<20 || s[10]!='T') return false;
    const auto time=s.substr(0,end);
    // RFC 5424 uses a dot separator and up to six fractional digits.
    const auto dot=time.find('.');
    if(time.find(',')!=time.npos) return false;
    if(dot!=time.npos) {auto p=dot+1; while(p<time.size() && time[p]>='0' && time[p]<='9') ++p; if(p==dot+1 || p-dot-1>6) return false;}
    if(!timestamp(time,false,result,true)) return false;
    s.remove_prefix(end+1);
    // A header has hostname, app-name, procid, msgid and structured-data.
    for(int i=0;i<4;++i) {const auto fieldEnd=s.find(' '); if(fieldEnd==s.npos || !fieldEnd) return false; s.remove_prefix(fieldEnd+1);}
    return !s.empty() && (s.front()=='-' || s.front()=='[');
}

}
LogCorrelationResult correlateLogs(const std::vector<LogInput>& inputs, std::string_view query, std::size_t maxBytes, std::size_t maxLines, std::size_t maxRecords, LogCorrelationOptions options, const std::atomic_bool* cancel) {
    LogCorrelationResult out; std::size_t bytes = 0, sequence = 0;
    constexpr std::size_t maxInputs = 64, maxSourceBytes = 4096, maxQueryBytes = 65536;
    maxBytes = std::min<std::size_t>(maxBytes, 16U * 1024U * 1024U);
    maxLines = std::min<std::size_t>(maxLines, 100000);
    maxRecords = std::min<std::size_t>(maxRecords, 10000);
    if (query.size() > maxQueryBytes || (cancel && cancel->load())) { out.incomplete = true; return out; }
    struct OrderedRecord { LogRecord record; std::size_t sequence; };
    std::vector<OrderedRecord> earliest;
    const auto before = [](const OrderedRecord& a, const OrderedRecord& b) {
        return a.record.timestampMillis < b.record.timestampMillis ||
            (a.record.timestampMillis == b.record.timestampMillis && a.sequence < b.sequence);
    };
    const auto inputCount = std::min(inputs.size(), maxInputs);
    if (inputs.size() > maxInputs) out.incomplete = true;
    for (std::size_t inputIndex = 0; inputIndex < inputCount; ++inputIndex) {
        if (cancel && cancel->load()) { out.incomplete = true; break; }
        const auto& input = inputs[inputIndex];
        if (input.source.size() > maxSourceBytes) { out.incomplete = true; continue; }
        std::size_t pos = 0, line = 0; LogRecord pending; bool hasPending = false;
        auto flush = [&] {
            if (!hasPending) return;
            if (query.empty() || pending.text.find(query) != std::string::npos || pending.source.find(query) != std::string::npos) {
                OrderedRecord candidate{std::move(pending), sequence++};
                if (earliest.size() < maxRecords) { earliest.push_back(std::move(candidate)); std::push_heap(earliest.begin(), earliest.end(), before); }
                else {
                    out.incomplete = true;
                    if (!earliest.empty() && before(candidate, earliest.front())) {
                        std::pop_heap(earliest.begin(), earliest.end(), before); earliest.back() = std::move(candidate); std::push_heap(earliest.begin(), earliest.end(), before);
                    }
                }
            }
            hasPending = false;
        };
        while (pos < input.text.size()) {
            if ((cancel && cancel->load()) || bytes >= maxBytes || out.scannedLines >= maxLines) { out.incomplete = true; break; }
            auto remaining = maxBytes - bytes;
            const auto window = input.text.substr(pos, std::min(remaining, input.text.size() - pos));
            const auto relativeNewline = window.find('\n');
            if (relativeNewline == std::string_view::npos && window.size() < input.text.size() - pos) { out.incomplete = true; break; }
            auto newline = relativeNewline == std::string_view::npos ? std::string_view::npos : pos + relativeNewline;
            auto end = newline == std::string_view::npos ? input.text.size() : newline;
            auto consumed = end - pos + (newline == std::string_view::npos ? 0 : 1);
            if (consumed > remaining) { out.incomplete = true; break; }
            bytes += consumed; ++line; ++out.scannedLines;
            auto text = input.text.substr(pos, end - pos); if (!text.empty() && text.back() == '\r') text.remove_suffix(1);
            pos = end + (newline == std::string_view::npos ? 0 : 1);
            std::int64_t time = 0; auto timestampText = text; const bool bracketed = !timestampText.empty() && timestampText.front() == '[';
            if (bracketed) timestampText.remove_prefix(1);
            auto trimmed=text; while(!trimmed.empty() && (trimmed.front()==' ' || trimmed.front()=='\t')) trimmed.remove_prefix(1);
            const bool jsonCandidate=options.enableJsonLines && !trimmed.empty() && trimmed.front()=='{';
            const bool isJson=jsonCandidate && jsonTimestamp(trimmed,time);
            if (isJson || timestamp(timestampText, bracketed, time) || rfc5424Timestamp(text, time) || syslogTimestamp(text, options, time)) {
                if(isJson) ++out.jsonLines;
                flush(); pending = {input.source, std::string(text), line, time}; hasPending = true;
            } else if (!jsonCandidate && hasPending && !text.empty() && (text.front() == ' ' || text.front() == '\t')) {
                pending.text.push_back('\n'); pending.text.append(text);
            } else { flush(); ++out.unparseableLines; }

        }
        flush();
        if (pos < input.text.size()) break;
    }
    std::sort(earliest.begin(), earliest.end(), before);
    out.records.reserve(earliest.size());
    for (auto& item : earliest) out.records.push_back(std::move(item.record));
    return out;
}
}
