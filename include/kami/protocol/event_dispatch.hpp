#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace kami::protocol::caps {
struct GroupSnapshot {
    std::string id;      // authenticated agent group UUID
    std::wstring name;   // not logged; private group display name
    std::string powers;  // encoded LLSD powers; not activated as permissions
    std::string insignia;
    bool notices=false;
    int contribution=0;
};
// A whole-batch, all-or-nothing validated session-state snapshot.
// A failed or unknown event MUST NOT be acknowledged to the simulator.
struct EventCommit {
    bool accepted=false;
    bool navmesh_state_seen=false;
    bool can_modify_navmesh=false;
    bool groups_seen=false;
    std::vector<GroupSnapshot> groups;
    std::size_t recognized_events=0;
};
[[nodiscard]] bool validate_session_events(
    std::string_view complete_llsd_response,
    std::string_view authenticated_agent_uuid,
    EventCommit& result);
}
