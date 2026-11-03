#pragma once
#include "kami/protocol/bootstrap.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kami::protocol {
// These are typed request contracts, NOT implementations of SL wire packets.
// Their results cannot become successful without authenticated server events.
enum class Service : std::uint8_t {
    Inventory, NearbyChat, InstantMessage, Friends, Groups,
    Teleport, WorldMap, Parcels, Building, AvatarAppearance,
    AssetDownload, Animation, Sound, Voice, Count
};
enum class IntentState : std::uint8_t {
    Pending, Sent, ServerConfirmed, ServerRejected
};
struct Intent {
    Service service=Service::Inventory;
    std::string target_id;
    std::wstring text;
    std::uint64_t id=0;
    std::uint64_t connection_generation=0;
    IntentState state=IntentState::Pending;
};
[[nodiscard]] const wchar_t* service_name(Service value) noexcept;
[[nodiscard]] bool validate_intent(const Intent& intent) noexcept;
// The caller owns the Bootstrap. A session reset revokes all outstanding
// requests automatically via its generation number.
class Services final {
public:
    explicit Services(const Bootstrap& session) noexcept:session_(session){}
    [[nodiscard]] std::uint64_t prepare(Service type,std::string target_id,
                                        std::wstring text);
    [[nodiscard]] bool mark_sent(std::uint64_t id) noexcept;
    [[nodiscard]] bool accept_server_result(std::uint64_t id,
                                            bool verified_server_reply,
                                            bool success) noexcept;
    void clear() noexcept;
    [[nodiscard]] const std::vector<Intent>& entries() const noexcept {
        return requests_;
    }
    [[nodiscard]] std::size_t pending_count() const noexcept;
private:
    const Bootstrap& session_;
    std::vector<Intent> requests_;
    std::uint64_t next_id_=1;
};

// The online world and inventory graph must be populated by authenticated,
// separately decoded simulator/asset messages. No fake content is accepted.
struct WorldObject {
    std::string object_id;
    std::string mesh_asset_id;
    float x=0.f,y=0.f,z=0.f;
};
struct InventoryEntry {
    std::string item_id;
    std::string parent_id;
    std::string asset_id;
    std::wstring name;
    std::uint32_t permissions=0;
};
class LiveData final {
public:
    explicit LiveData(const Bootstrap& session) noexcept:session_(session){}
    [[nodiscard]] bool apply_objects(std::vector<WorldObject> value,
                                     bool verified_simulator_data);
    [[nodiscard]] bool apply_inventory(std::vector<InventoryEntry> value,
                                       bool verified_account_data);
    void clear() noexcept;
    // Invalidate views across disconnect/re-login without displaying stale
    // avatar, region or inventory data from an earlier session generation.
    [[nodiscard]] const std::vector<WorldObject>& objects() const noexcept {
        static const std::vector<WorldObject> empty;
        return session_.online() && objects_generation_==session_.generation()?
            objects_:empty;
    }
    [[nodiscard]] const std::vector<InventoryEntry>& inventory() const noexcept {
        static const std::vector<InventoryEntry> empty;
        return session_.online() && inventory_generation_==session_.generation()?
            inventory_:empty;
    }
private:
    const Bootstrap& session_;
    std::uint64_t objects_generation_=0,inventory_generation_=0;
    std::vector<WorldObject> objects_;
    std::vector<InventoryEntry> inventory_;
};
} // namespace kami::protocol
