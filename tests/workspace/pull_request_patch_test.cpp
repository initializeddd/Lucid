#include "modules/PullRequestPatch.hpp"
#include <string>
#include <vector>
using namespace lucid::modules;
int main() {
    std::vector<PatchHunk> hunks; std::string error;
    if (!parsePullRequestPatch("@@ -2,2 +2,2 @@ title\n same\n-old\n+new\n\\ No newline at end of file\n", hunks, error) ||
        hunks.size() != 1 || hunks[0].lines.size() != 4 || hunks[0].lines[0].oldLine != 2 ||
        hunks[0].lines[1].oldLine != 3 || hunks[0].lines[2].newLine != 3 || hunks[0].lines[3].kind != PatchLineKind::NoNewline) return 1;
    if (!parsePullRequestPatch("@@ -0,0 +1,1 @@\n+new\n", hunks, error) || hunks[0].lines[0].newLine != 1) return 2;
    if (parsePullRequestPatch("@@ -1,2 +1,2 @@\n one\n", hunks, error) || !hunks.empty()) return 3;
    if (parsePullRequestPatch("@@ -1 +1 @@\n one\n+extra\n", hunks, error) || !hunks.empty()) return 4;
    if (parsePullRequestPatch("@@ -18446744073709551615,1 +1 @@\n-a\n+b\n", hunks, error) || !hunks.empty()) return 5;
    if (parsePullRequestPatch(std::string(2U * 1024U * 1024U + 1, 'x'), hunks, error) || !hunks.empty()) return 6;
    return 0;
}
