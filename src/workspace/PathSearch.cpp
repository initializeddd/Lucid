#include "workspace/PathSearch.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace lucid::workspace {
namespace {

struct ScoredPath {
    std::string_view path;
    int score = 0;
};

[[nodiscard]] char lowerAscii(char value) noexcept {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

[[nodiscard]] bool equalsIgnoreCase(std::string_view left, std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (lowerAscii(left[index]) != lowerAscii(right[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool containsIgnoreCase(std::string_view text, std::string_view query) noexcept {
    if (query.empty()) {
        return true;
    }
    if (query.size() > text.size()) {
        return false;
    }

    for (std::size_t start = 0; start + query.size() <= text.size(); ++start) {
        bool matched = true;
        for (std::size_t offset = 0; offset < query.size(); ++offset) {
            if (lowerAscii(text[start + offset]) != lowerAscii(query[offset])) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string_view basename(std::string_view path) noexcept {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

[[nodiscard]] int scorePath(std::string_view path, std::string_view query) {
    if (query.empty()) {
        return 0;
    }

    std::size_t queryIndex = 0;
    std::int64_t score = 0;
    std::size_t previousMatch = 0;
    bool hasPreviousMatch = false;

    for (std::size_t index = 0; index < path.size() && queryIndex < query.size(); ++index) {
        if (lowerAscii(path[index]) != lowerAscii(query[queryIndex])) {
            continue;
        }

        score += 10;
        if (hasPreviousMatch && previousMatch + 1 == index) {
            score += 12;
        }
        if (index == 0 || path[index - 1] == '/' || path[index - 1] == '\\') {
            score += 18;
        }

        previousMatch = index;
        hasPreviousMatch = true;
        ++queryIndex;
    }

    if (queryIndex != query.size()) {
        return -1;
    }

    if (equalsIgnoreCase(path, query)) {
        score += 10000;
    } else if (equalsIgnoreCase(basename(path), query)) {
        score += 1000;
    } else if (containsIgnoreCase(path, query)) {
        score += 120;
    }

    score -= static_cast<std::int64_t>(path.size() / 4);
    return static_cast<int>(std::clamp<std::int64_t>(score, -1000000, 1000000));
}

} // namespace

std::vector<std::string_view> filterPaths(const std::vector<std::string>& paths,
                                          std::string_view query, std::size_t limit) {
    std::vector<ScoredPath> scored;
    scored.reserve(paths.size());

    for (const std::string& path : paths) {
        if (query.empty()) {
            scored.push_back(ScoredPath{path, 0});
            continue;
        }

        const int score = scorePath(path, query);
        if (score >= 0) {
            scored.push_back(ScoredPath{path, score});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredPath& left, const ScoredPath& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        if (left.path.size() != right.path.size()) {
            return left.path.size() < right.path.size();
        }
        return left.path < right.path;
    });

    if (scored.size() > limit) {
        scored.resize(limit);
    }

    std::vector<std::string_view> result;
    result.reserve(scored.size());
    for (const ScoredPath& match : scored) {
        result.push_back(match.path);
    }
    return result;
}

} // namespace lucid::workspace
