#include "modules/DiagramPreview.hpp"
#include <map>
#include <set>
#include <algorithm>
#include <queue>
#include <functional>
namespace lucid::modules {
namespace {
constexpr std::size_t maxBytes=1024U*1024U, maxItems=10000;
std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front()==' ' || s.front()=='\t' || s.front()=='\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back()==' ' || s.back()=='\t' || s.back()=='\r')) s.remove_suffix(1);
    return s;
}
bool letter(char c) { return (c>='A'&&c<='Z') || (c>='a'&&c<='z') || c=='_'; }
bool digit(char c) { return c>='0'&&c<='9'; }
bool plainText(std::string_view text, std::size_t limit, bool edgeLabel = false) {
    if (text.empty() || text.size()>limit || trim(text).empty()) return false;
    for (unsigned char c : text) if (c<32 || c==127 || c=='<' || c=='>' || c=='[' || c==']' || c=='"' || c=='\'' || c=='\\' || c=='&' || (edgeLabel && c=='|')) return false;
    return true;
}
struct Arrow { std::size_t position=std::string_view::npos, length=0; bool bidirectional=false; };
Arrow findArrow(std::string_view text) {
    const auto position=text.find("-->"); if(position==text.npos) return {};
    if(position>0 && text[position-1]=='<') return {position-1,4,true};
    return {position,3,false};
}
struct ParsedNode { std::string id, label; bool explicitLabel=false; DiagramNodeShape shape=DiagramNodeShape::Rectangle; };
bool node(std::string_view s, ParsedNode& out) {
    s=trim(s); if (s.empty() || !letter(s.front())) return false;
    std::size_t i=1; while (i<s.size() && (letter(s[i]) || digit(s[i]))) ++i;
    if (i > 256) return false;
    out.id=std::string(s.substr(0,i)); s=trim(s.substr(i));
    if (s.empty()) { out.label=out.id; return true; }
    std::string_view label;
    if(s.size()>=5 && s.starts_with("([") && s.ends_with("])")) {out.shape=DiagramNodeShape::Stadium;label=s.substr(2,s.size()-4);if(label.find_first_of("()")!=label.npos)return false;}
    else if(s.size()>=3 && s.front()=='[' && s.back()==']') {out.shape=DiagramNodeShape::Rectangle;label=s.substr(1,s.size()-2);}
    else if(s.size()>=3 && s.front()=='(' && s.back()==')' && !s.starts_with("((") && !s.ends_with("))")) {out.shape=DiagramNodeShape::Rounded;label=s.substr(1,s.size()-2);if(label.find_first_of("()")!=label.npos)return false;}
    else return false;
    if (!plainText(label, 4096)) return false;
    out.label=std::string(label); out.explicitLabel=true; return true;
}
bool nodeGroup(std::string_view text,std::vector<ParsedNode>& group) {
    group.clear(); std::set<std::string> ids;
    for (;;) {
        const auto separator=text.find('&'); ParsedNode item;
        if(!node(text.substr(0,separator),item) || !ids.insert(item.id).second || group.size()>=maxItems) return false;
        group.push_back(std::move(item)); if(separator==text.npos) return true; text=text.substr(separator+1);
    }
}
}
bool parseMermaidFlowchart(std::string_view source, DiagramGraph& graph, std::string& error, const std::atomic_bool* cancel) {
    graph={}; error.clear(); if (cancel && cancel->load()) { error="Diagram parsing cancelled"; return false; } if (source.size()>maxBytes) { error="Diagram exceeds size limit"; return false; }
    DiagramGraph result; std::map<std::string,std::size_t> indexes; std::map<std::string,bool> explicitLabels;
    std::size_t pos=0, line=0; bool header=false;
    auto bad=[&](std::string message) { error=std::move(message)+" on line "+std::to_string(line); return false; };
    auto add=[&](const ParsedNode& n) {
        auto it=indexes.find(n.id);
        if (it==indexes.end()) { if (result.nodes.size()>=maxItems) return false; indexes[n.id]=result.nodes.size(); explicitLabels[n.id]=n.explicitLabel; result.nodes.push_back({n.id,n.label,n.shape}); return true; }
        if (n.explicitLabel && explicitLabels[n.id] && (result.nodes[it->second].label!=n.label || result.nodes[it->second].shape!=n.shape)) return false;
        if (n.explicitLabel) { result.nodes[it->second].label=n.label; result.nodes[it->second].shape=n.shape; explicitLabels[n.id]=true; }
        return true;
    };
    while (pos<source.size()) {
        if (cancel && cancel->load()) { error="Diagram parsing cancelled"; return false; }
        ++line; auto end=source.find('\n',pos); if(end==source.npos) end=source.size();
        auto s=trim(source.substr(pos,end-pos)); pos=end==source.size()?end:end+1;
        if(s.empty()) continue;
        if(s.starts_with("%%{")) return bad("Unsupported Mermaid directive");
        if(s.starts_with("%%")) continue;
        if(!header) {
            auto separator=s.find_first_of(" \t"); if(separator==s.npos) return bad("Expected flowchart header");
            auto kind=s.substr(0,separator), direction=trim(s.substr(separator));
            if((kind!="flowchart" && kind!="graph") || (direction!="TD" && direction!="TB" && direction!="BT" && direction!="LR" && direction!="RL")) return bad("Unsupported flowchart header");
            result.direction=std::string(direction); header=true; continue;
        }
        auto arrow=findArrow(s); ParsedNode a;
        if(arrow.position==s.npos) { if(!node(s,a) || !add(a)) return bad("Unsupported node or conflicting label"); }
        else {
            std::vector<ParsedNode> sources;
            if(!nodeGroup(s.substr(0,arrow.position),sources)) return bad("Unsupported edge group");
            for(const auto& item:sources) if(!add(item)) return bad("Too many nodes or conflicting labels");
            auto remainder=s.substr(arrow.position+arrow.length);
            for (;;) {
                if(cancel && cancel->load()) {error="Diagram parsing cancelled";return false;}
                auto target=trim(remainder); std::string label;
                if(target.starts_with('|')) {
                    const auto closing=target.find('|',1);
                    if(closing==target.npos || !plainText(target.substr(1,closing-1),256,true)) return bad("Unsupported edge label");
                    label=std::string(target.substr(1,closing-1)); target=trim(target.substr(closing+1));
                }
                const auto nextArrow=findArrow(target); std::vector<ParsedNode> targets;
                if(!nodeGroup(target.substr(0,nextArrow.position),targets)) return bad("Unsupported edge group");
                for(const auto& item:targets) if(!add(item)) return bad("Too many nodes or conflicting labels");
                if(targets.size()>(maxItems-result.edges.size())/sources.size()) return bad("Too many edges");
                for(const auto& origin:sources) for(const auto& destination:targets) {
                    if(cancel && cancel->load()) {error="Diagram parsing cancelled";return false;}
                    result.edges.push_back({origin.id,destination.id,label,arrow.bidirectional});
                }
                sources=std::move(targets);
                if(nextArrow.position==target.npos) break;
                arrow=nextArrow; remainder=target.substr(nextArrow.position+nextArrow.length);
            }
        }
    }
    if(!header) { error="Missing flowchart header"; return false; }
    graph=std::move(result); return true;
}
bool layoutMermaidFlowchart(const DiagramGraph& graph, DiagramLayout& layout, std::string& error, const std::atomic_bool* cancel) {
    layout = {}; error.clear();
    if (cancel && cancel->load()) { error="Diagram layout cancelled"; return false; }
    const bool horizontal = graph.direction == "LR" || graph.direction == "RL";
    const bool reverse = graph.direction == "RL" || graph.direction == "BT";
    if (!horizontal && graph.direction != "TD" && graph.direction != "TB" && graph.direction != "BT") { error = "Unsupported layout direction"; return false; }
    if (graph.nodes.size() > 300 || graph.edges.size() > 1000) { error = "Diagram exceeds layout limit"; return false; }
    std::map<std::string,std::size_t> indices;
    for (std::size_t i=0;i<graph.nodes.size();++i) {
        if (cancel && cancel->load()) { error="Diagram layout cancelled"; return false; }
        const auto& item = graph.nodes[i];
        if (item.id.empty() || item.id.size()>256 || !letter(item.id.front()) ||
            !std::all_of(item.id.begin()+1,item.id.end(),[](char c){ return letter(c) || digit(c); }) ||
            !plainText(item.label,4096) || (item.shape!=DiagramNodeShape::Rectangle && item.shape!=DiagramNodeShape::Rounded && item.shape!=DiagramNodeShape::Stadium) || !indices.emplace(item.id,i).second) {
            error = "Invalid, oversized or duplicate node"; return false;
        }
    }
    const auto n=graph.nodes.size(); std::vector<std::vector<std::size_t>> next(n); std::vector<std::size_t> level(n);
    for (const auto& e:graph.edges) {
        if (cancel && cancel->load()) { error="Diagram layout cancelled"; return false; }
        if (e.from.size()>256 || e.to.size()>256 || (!e.label.empty() && !plainText(e.label,256,true))) { error="Invalid or oversized edge"; return false; }
        auto a=indices.find(e.from), b=indices.find(e.to);
        if(a==indices.end() || b==indices.end()) { error="Edge references missing node"; return false; }
        next[a->second].push_back(b->second); if(e.bidirectional && a->second!=b->second) next[b->second].push_back(a->second);
    }
    // Collapse strongly connected components before assigning layers. Graphs
    // are limited to 300 nodes, so bounded recursive DFS is safe here.
    std::vector<std::vector<std::size_t>> reverseEdges(n);
    for(std::size_t i=0;i<n;++i) for(auto j:next[i]) reverseEdges[j].push_back(i);
    std::vector<bool> seen(n); std::vector<std::size_t> order; order.reserve(n); bool interrupted=false;
    std::function<void(std::size_t)> visit=[&](std::size_t i) { if(cancel && cancel->load()) {interrupted=true;return;} seen[i]=true; for(auto j:next[i]) if(!seen[j]) visit(j); order.push_back(i); };
    for(std::size_t i=0;i<n && !interrupted;++i) if(!seen[i]) visit(i);
    if(interrupted) {error="Diagram layout cancelled";return false;}
    std::vector<std::size_t> component(n,n); std::size_t componentCount=0;
    std::function<void(std::size_t)> assign=[&](std::size_t i) { if(cancel && cancel->load()) {interrupted=true;return;} component[i]=componentCount; for(auto j:reverseEdges[i]) if(component[j]==n) assign(j); };
    for(auto it=order.rbegin();it!=order.rend() && !interrupted;++it) if(component[*it]==n) {assign(*it);++componentCount;}
    if(interrupted) {error="Diagram layout cancelled";return false;}
    std::vector<std::vector<std::size_t>> componentEdges(componentCount); std::vector<std::size_t> componentIn(componentCount), componentLevel(componentCount);
    for(std::size_t i=0;i<n;++i) for(auto j:next[i]) if(component[i]!=component[j]) {componentEdges[component[i]].push_back(component[j]);++componentIn[component[j]];}
    std::queue<std::size_t> ready; for(std::size_t i=0;i<componentCount;++i) if(!componentIn[i]) ready.push(i);
    std::size_t visited=0, maxLevel=0;
    while(!ready.empty()) { if (cancel && cancel->load()) { error="Diagram layout cancelled"; return false; } auto i=ready.front(); ready.pop(); ++visited; for(auto j:componentEdges[i]) { componentLevel[j]=std::max(componentLevel[j],componentLevel[i]+1); maxLevel=std::max(maxLevel,componentLevel[j]); if(!--componentIn[j]) ready.push(j); } }
    if(visited!=componentCount) {error="Could not lay out diagram components";return false;}
    for(std::size_t i=0;i<n;++i) level[i]=componentLevel[component[i]];
    std::vector<std::size_t> rows(n ? maxLevel+1 : 0); DiagramLayout out; out.nodes.reserve(n);
    constexpr double margin=24, w=140, h=60, layerGap=100, rowGap=40;
    for(std::size_t i=0;i<n;++i) {
        if (cancel && cancel->load()) { error="Diagram layout cancelled"; return false; }
        const auto layer=reverse ? maxLevel-level[i] : level[i]; const auto row=rows[level[i]]++;
        const double x=margin+(horizontal ? layer*(w+layerGap) : row*(w+rowGap));
        const double y=margin+(horizontal ? row*(h+rowGap) : layer*(h+layerGap));
        out.nodes.push_back({graph.nodes[i].id,graph.nodes[i].label,x,y,w,h,graph.nodes[i].shape});
        out.width=std::max(out.width,x+w+margin); out.height=std::max(out.height,y+h+margin);
    }
    std::size_t detourCount=0; double requiredWidth=out.width, requiredHeight=out.height;
    for(const auto& e:graph.edges) {
        if (cancel && cancel->load()) { error="Diagram layout cancelled"; return false; }
        const auto& a=out.nodes[indices.at(e.from)]; const auto& b=out.nodes[indices.at(e.to)];
        const bool longEdge=level[indices.at(e.to)] > level[indices.at(e.from)]+1;
        DiagramPositionedEdge edge; edge.from=e.from; edge.to=e.to; edge.label=e.label; edge.bidirectional=e.bidirectional;
        const bool cycleEdge=component[indices.at(e.from)]==component[indices.at(e.to)];
        if(cycleEdge && horizontal) {
            const double ax=a.x+w, bx=b.x+w, ay=a.y+h/2, by=b.y+h/2;
            const double lane=out.height+20+20*detourCount++;
            edge.points={{ax,ay},{ax+20,ay},{ax+20,lane},{bx+40,lane},{bx+40,by},{bx,by}};
        } else if(cycleEdge) {
            const double ay=a.y+h, by=b.y+h, ax=a.x+w/2, bx=b.x+w/2;
            const double lane=out.width+20+20*detourCount++;
            edge.points={{ax,ay},{ax,ay+20},{lane,ay+20},{lane,by+40},{bx,by+40},{bx,by}};
        } else if(horizontal) {
            const double ax=reverse?a.x:a.x+w, bx=reverse?b.x+w:b.x, ay=a.y+h/2, by=b.y+h/2, mid=(ax+bx)/2;
            if (longEdge) {
                // Cross intervening layers outside the nodes, rather than through them.
                const double lane=out.height+20+20*detourCount++;
                const double exit=ax+(reverse?-layerGap/2:layerGap/2);
                const double entry=bx+(reverse?layerGap/2:-layerGap/2);
                edge.points={{ax,ay},{exit,ay},{exit,lane},{entry,lane},{entry,by},{bx,by}};
            } else edge.points={{ax,ay},{mid,ay},{mid,by},{bx,by}};
        } else {
            const double ay=reverse?a.y:a.y+h, by=reverse?b.y+h:b.y, ax=a.x+w/2, bx=b.x+w/2, mid=(ay+by)/2;
            if (longEdge) {
                const double lane=out.width+20+20*detourCount++;
                const double exit=ay+(reverse?-layerGap/2:layerGap/2);
                const double entry=by+(reverse?layerGap/2:-layerGap/2);
                edge.points={{ax,ay},{ax,exit},{lane,exit},{lane,entry},{bx,entry},{bx,by}};
            } else edge.points={{ax,ay},{ax,mid},{bx,mid},{bx,by}};
        }
        for(const auto& point:edge.points) {requiredWidth=std::max(requiredWidth,point.x+margin);requiredHeight=std::max(requiredHeight,point.y+margin);}
        out.edges.push_back(std::move(edge));
    }
    if (detourCount) {
        if (horizontal) out.height+=20+20*(detourCount-1)+margin;
        else out.width+=20+20*(detourCount-1)+margin;
    }
    out.width=std::max(out.width,requiredWidth); out.height=std::max(out.height,requiredHeight);
    layout=std::move(out); return true;
}

}
