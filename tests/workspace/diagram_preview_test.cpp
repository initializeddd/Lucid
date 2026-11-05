#include "modules/DiagramPreview.hpp"
#include <algorithm>
#include <string>
using namespace lucid::modules;
int main() {
    DiagramGraph graph; std::string error;
    if (!parseMermaidFlowchart("flowchart LR\n A[Start] --> B\n B --> C[Done]\n B[Middle]\n", graph, error) || graph.direction!="LR" || graph.nodes.size()!=3 || graph.edges.size()!=2 || graph.nodes[1].label!="Middle") return 1;
    if (!parseMermaidFlowchart("graph TD\nA --> B --> C",graph,error) || graph.nodes.size()!=3 || graph.edges.size()!=2 || graph.edges[0].from!="A" || graph.edges[1].from!="B" || graph.edges[1].to!="C") return 2;
    if (parseMermaidFlowchart("flowchart LR\nclick A \"javascript:x\"",graph,error)) return 3;
    if (parseMermaidFlowchart("flowchart LR\nA[<script>]",graph,error)) return 4;
    if (parseMermaidFlowchart("flowchart LR\nA[one]\nA[two]",graph,error)) return 5;
    if (parseMermaidFlowchart("sequenceDiagram\nA->B",graph,error)) return 6;
    if (parseMermaidFlowchart(std::string(1024U*1024U+1,'x'),graph,error)) return 7;
    if (!parseMermaidFlowchart("%% comment\ngraph TD\nA\n",graph,error) || graph.nodes[0].label!="A") return 8;
    if (parseMermaidFlowchart("%%{init: {}}%%\ngraph TD\nA",graph,error)) return 9;
    if (!parseMermaidFlowchart("flowchart LR\nA --> B\nA --> C\nC --> D",graph,error)) return 10;
    DiagramLayout layout;
    if (!layoutMermaidFlowchart(graph,layout,error) || layout.nodes.size()!=4 || layout.edges.size()!=3 || layout.nodes[0].x>=layout.nodes[1].x || layout.edges[0].points.size()!=4 || layout.width<=0) return 11;
    graph.direction="RL"; if (!layoutMermaidFlowchart(graph,layout,error) || layout.nodes[0].x<=layout.nodes[1].x) return 12;
    graph.direction="TD"; if (!layoutMermaidFlowchart(graph,layout,error) || layout.nodes[0].y>=layout.nodes[1].y) return 13;
    graph.direction="BT"; if (!layoutMermaidFlowchart(graph,layout,error) || layout.nodes[0].y<=layout.nodes[1].y) return 14;
    graph.edges.push_back({"D","A"}); if (!layoutMermaidFlowchart(graph,layout,error) || layout.nodes.size()!=4 || layout.edges.size()!=4 || layout.edges.back().points.size()!=6) return 15;
    graph.edges={{"A","missing"}}; if (layoutMermaidFlowchart(graph,layout,error)) return 16;
    // A direct edge skipping a layer must detour around that layer's node.
    for (const auto* direction : {"LR", "RL", "TD", "BT"}) {
        const std::string source = std::string("flowchart ") + direction + "\nA --> B\nB --> C\nA --> C\n";
        if (!parseMermaidFlowchart(source, graph, error) || !layoutMermaidFlowchart(graph, layout, error)) return 17;
        const auto& skipped = layout.nodes[1];
        const auto& edge = layout.edges[2];
        if (edge.points.size()!=6) return 18;
        for (const auto& point : edge.points) if (point.x < 0 || point.y < 0 || point.x > layout.width || point.y > layout.height) return 19;
        for (std::size_t i=1; i<edge.points.size(); ++i) {
            const auto a=edge.points[i-1], b=edge.points[i];
            if (a.x == b.x && a.x > skipped.x && a.x < skipped.x+skipped.width &&
                std::max(a.y,b.y) > skipped.y && std::min(a.y,b.y) < skipped.y+skipped.height) return 20;
            if (a.y == b.y && a.y > skipped.y && a.y < skipped.y+skipped.height &&
                std::max(a.x,b.x) > skipped.x && std::min(a.x,b.x) < skipped.x+skipped.width) return 21;
        }
    }
    if (parseMermaidFlowchart("flowchart LR\n" + std::string(257, 'A'),graph,error) || !graph.nodes.empty()) return 22;
    if (parseMermaidFlowchart(std::string("flowchart LR\nA[bad") + char(127) + "]",graph,error)) return 23;
    graph.direction="LR"; graph.nodes={{std::string(257, 'a'),"label"}}; graph.edges.clear();
    if (layoutMermaidFlowchart(graph,layout,error) || !layout.nodes.empty()) return 24;
    graph.nodes={{"ok",std::string(4097, 'a')}};
    if (layoutMermaidFlowchart(graph,layout,error) || !layout.nodes.empty()) return 25;
    graph.nodes={{"ok","label"}}; graph.edges={{"ok",std::string(257, 'a')}};
    if (layoutMermaidFlowchart(graph,layout,error) || !layout.nodes.empty()) return 26;
    if (!parseMermaidFlowchart("flowchart LR\nA -->|next step| B\n", graph, error) || graph.edges.size()!=1 || graph.edges[0].label!="next step" ||
        !layoutMermaidFlowchart(graph,layout,error) || layout.edges[0].label!="next step") return 27;
    if (parseMermaidFlowchart("flowchart LR\nA -->|<img>| B\n",graph,error) ||
        parseMermaidFlowchart("flowchart LR\nA -->|| B\n",graph,error) ||
        parseMermaidFlowchart("flowchart LR\nA -->|" + std::string(257,'x') + "| B\n",graph,error)) return 28;
    for(const auto* direction:{"LR","RL","TD","BT"}) {
        if(!parseMermaidFlowchart(std::string("flowchart ")+direction+"\nA --> B\nB --> A\nA --> A\nB --> C\n",graph,error) || !layoutMermaidFlowchart(graph,layout,error) || layout.edges.size()!=4 || layout.nodes.size()!=3) return 33;
        for(const auto& edge:layout.edges) {if(edge.points.size()<4) return 34; for(const auto& point:edge.points) if(point.x<0 || point.y<0 || point.x>layout.width || point.y>layout.height) return 35;}
        const auto& loop=layout.edges[2]; if(loop.points.front().x!=loop.points.back().x || loop.points.front().y!=loop.points.back().y || loop.points[1].x==loop.points[3].x && loop.points[1].y==loop.points[3].y) return 36;
    }
    DiagramGraph large; large.direction="LR"; for(int i=0;i<300;++i) large.nodes.push_back({"n"+std::to_string(i),"node"});
    for(int i=0;i<300;++i) large.edges.push_back({"n"+std::to_string(i),"n"+std::to_string((i+1)%300),{}});
    DiagramLayout first,second; if(!layoutMermaidFlowchart(large,first,error) || !layoutMermaidFlowchart(large,second,error) || first.nodes.size()!=300 || first.edges.size()!=300 || first.width!=second.width || first.height!=second.height) return 37;
    for(std::size_t i=0;i<first.nodes.size();++i) if(first.nodes[i].x!=second.nodes[i].x || first.nodes[i].y!=second.nodes[i].y) return 38;
    if(!parseMermaidFlowchart("flowchart LR\nA[Start] -->|yes| B[Middle] -->|next| C[End] --> A\n",graph,error) || graph.nodes.size()!=3 || graph.edges.size()!=3 || graph.edges[0].label!="yes" || graph.edges[1].label!="next" || graph.edges[2].to!="A" || !layoutMermaidFlowchart(graph,layout,error)) return 39;
    for(const auto* malformed:{"flowchart LR\nA --> B -->", "flowchart LR\nA -->|yes| B -->|| C", "flowchart LR\nA --> B --> C[<script>]", "flowchart LR\nA --> B --> A[one]\nA[two]"}) if(parseMermaidFlowchart(malformed,graph,error) || !graph.nodes.empty()) return 40;
    std::string oversizeChain="flowchart LR\nA"; for(int i=0;i<10001;++i) oversizeChain+=" --> A";
    if(parseMermaidFlowchart(oversizeChain,graph,error) || !graph.nodes.empty() || error.find("Too many")==std::string::npos) return 41;
    if(!parseMermaidFlowchart("flowchart LR\nA <--> B -->|then| C <--> |back| D\n",graph,error) || graph.edges.size()!=3 || !graph.edges[0].bidirectional || graph.edges[1].bidirectional || !graph.edges[2].bidirectional || graph.edges[2].label!="back") return 42;
    if(!layoutMermaidFlowchart(graph,layout,error) || layout.edges.size()!=3 || !layout.edges[0].bidirectional || layout.edges[1].bidirectional || !layout.edges[2].bidirectional || layout.nodes[0].x!=layout.nodes[1].x) return 43;
    for(const auto* invalid:{"flowchart LR\nA <-- B", "flowchart LR\nA <<--> B", "flowchart LR\nA <-->|| B", "flowchart LR\nA <--> <script>"}) if(parseMermaidFlowchart(invalid,graph,error) || !graph.nodes.empty()) return 44;
    if(!parseMermaidFlowchart("flowchart LR\nA[First] & B -->|join| C & D <--> E\n",graph,error) || graph.nodes.size()!=5 || graph.edges.size()!=6) return 45;
    if(graph.edges[0].from!="A" || graph.edges[0].to!="C" || graph.edges[0].label!="join" || graph.edges[3].from!="B" || graph.edges[3].to!="D" || !graph.edges[4].bidirectional || !graph.edges[5].bidirectional || !layoutMermaidFlowchart(graph,layout,error)) return 46;
    for(const auto* invalid:{"flowchart LR\nA & --> B", "flowchart LR\nA --> B &", "flowchart LR\nA & A --> B", "flowchart LR\nA --> B & B", "flowchart LR\nA & <script> --> B"}) if(parseMermaidFlowchart(invalid,graph,error) || !graph.nodes.empty()) return 47;
    std::string expansion="flowchart LR\n";for(int i=0;i<101;++i) {if(i)expansion+=" & ";expansion+="a"+std::to_string(i);} expansion+=" --> ";for(int i=0;i<100;++i) {if(i)expansion+=" & ";expansion+="b"+std::to_string(i);} if(parseMermaidFlowchart(expansion,graph,error) || !graph.nodes.empty() || error.find("Too many edges")==std::string::npos) return 48;
    if(!parseMermaidFlowchart("flowchart LR\nA(Round) --> B([Start or stop]) --> C[Box]\n",graph,error) || graph.nodes.size()!=3 || graph.nodes[0].shape!=DiagramNodeShape::Rounded || graph.nodes[1].shape!=DiagramNodeShape::Stadium || graph.nodes[2].shape!=DiagramNodeShape::Rectangle || !layoutMermaidFlowchart(graph,layout,error) || layout.nodes[0].shape!=DiagramNodeShape::Rounded || layout.nodes[1].shape!=DiagramNodeShape::Stadium) return 49;
    for(const auto* invalid:{"flowchart LR\nA((Circle))", "flowchart LR\nA{Diamond}", "flowchart LR\nA(Round)\nA[Round]", "flowchart LR\nA([<script>])", "flowchart LR\nA(foo(bar))"}) if(parseMermaidFlowchart(invalid,graph,error) || !graph.nodes.empty()) return 50;
    graph={"LR",{{"a","A",static_cast<DiagramNodeShape>(99)}},{}};if(layoutMermaidFlowchart(graph,layout,error) || !layout.nodes.empty())return 51;
    std::atomic_bool cancel{true}; graph.nodes={{"old","old"}};
    if (parseMermaidFlowchart("flowchart LR\nA",graph,error,&cancel) || !graph.nodes.empty() || error.find("cancelled")==std::string::npos) return 29;
    cancel=false; if (!parseMermaidFlowchart("flowchart LR\nA --> B",graph,error,&cancel)) return 30;
    cancel=true; layout.nodes.push_back({});
    if (layoutMermaidFlowchart(graph,layout,error,&cancel) || !layout.nodes.empty() || error.find("cancelled")==std::string::npos) return 31;
    cancel=false; graph.nodes[0].label="bad<markup>";
    if (layoutMermaidFlowchart(graph,layout,error,&cancel) || !layout.nodes.empty()) return 32;
    return 0;
}
