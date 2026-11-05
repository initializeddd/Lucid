#pragma once
#include <atomic>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
enum class DiagramNodeShape { Rectangle, Rounded, Stadium };
struct DiagramNode { std::string id, label; DiagramNodeShape shape=DiagramNodeShape::Rectangle; };
struct DiagramEdge { std::string from, to; std::string label{}; bool bidirectional=false; };
struct DiagramGraph { std::string direction; std::vector<DiagramNode> nodes; std::vector<DiagramEdge> edges; };
// Supports flowchart/graph TD, TB, BT, LR, RL; one node or directed edge
// per line, optionally labeled as A -->|label| B; bidirectional <--> and chained links are
// supported (A --> B -->|label| C), as are branch groups (A & B --> C & D).
// Branches expand to the Cartesian product of each linked group. Identifiers [A-Za-z_][A-Za-z0-9_]* and
// optional plain [label], (label) rounded nodes, or ([label]) stadium nodes.
// Rejects unsupported syntax; no rendering, scripts, or network access.
[[nodiscard]] bool parseMermaidFlowchart(std::string_view source, DiagramGraph& graph, std::string& error, const std::atomic_bool* cancel = nullptr);
}

namespace lucid::modules {
struct DiagramPoint { double x = 0, y = 0; };
struct DiagramPositionedNode { std::string id, label; double x = 0, y = 0, width = 140, height = 60; DiagramNodeShape shape=DiagramNodeShape::Rectangle; };
struct DiagramPositionedEdge { std::string from, to; std::vector<DiagramPoint> points; std::string label; bool bidirectional=false; };
struct DiagramLayout { std::vector<DiagramPositionedNode> nodes; std::vector<DiagramPositionedEdge> edges; double width = 0, height = 0; };
// Deterministic layered layout including cycles and self-loops. Strongly
// connected nodes share a layer; cycle edges use outer detours. Rejects
// missing/duplicate nodes, unsupported directions and graphs over 300 nodes or
// 1000 edges. Layout is approximate and not equivalent to Mermaid rendering.
[[nodiscard]] bool layoutMermaidFlowchart(const DiagramGraph& graph, DiagramLayout& layout, std::string& error, const std::atomic_bool* cancel = nullptr);
}
