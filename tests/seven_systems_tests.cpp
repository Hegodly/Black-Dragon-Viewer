#include "kami/diagnostics/seven_systems.hpp"
#include <array>
#include <string_view>
int main(){
    using namespace kami::diagnostics;
    const auto empty=evaluate({});
    if(empty.size()!=7U)return 1;
    if(empty[0].stage!=Stage::Waiting||
       empty[1].stage!=Stage::Waiting||
       empty[2].stage!=Stage::Waiting||
       empty[3].stage!=Stage::Waiting||
       empty[4].stage!=Stage::LocalOnly||
       empty[5].stage!=Stage::Waiting||
       empty[6].stage!=Stage::LocalOnly)return 2;
    const Evidence observed{5U,true,true,4U,true,true};
    const auto live=evaluate(observed);
    for(unsigned i=0;i<live.size();++i){
        if(!live[i].title||!live[i].details||
           std::wstring_view(live[i].title).empty()||
           std::wstring_view(live[i].details).empty())return 3;
    }
    if(live[0].stage!=Stage::Partial||
       live[1].stage!=Stage::Partial||
       live[2].stage!=Stage::LocalOnly||
       live[3].stage!=Stage::Partial||
       live[4].stage!=Stage::LocalOnly||
       live[5].stage!=Stage::Partial||
       live[6].stage!=Stage::LocalOnly)return 4;
    auto no_auth=observed;no_auth.authenticated_simulator=false;
    if(evaluate(no_auth)[3].stage!=Stage::Waiting)return 5;
    if(std::wstring_view(stage_label(Stage::Partial))!=L"PARTIAL"||
       std::wstring_view(stage_label(Stage::LocalOnly))!=L"LOCAL ONLY")
        return 6;
    // Most important: object packet observation and placeholder rendering
    // do NOT get mislabeled as full live Second Life implementations.
    if(std::wstring_view(live[0].details).find(L"NOT implemented")==
       std::wstring_view::npos ||
       std::wstring_view(live[2].details).find(L"placeholder")==
       std::wstring_view::npos)return 7;
    return 0;
}
