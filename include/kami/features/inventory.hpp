#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "kami/protocol/caps.hpp"

namespace kami::inventory {
struct Item {
    std::wstring name;
    std::wstring creator;
    bool is_folder = false;
};
enum class SortMode : unsigned { Name=0, Creator=1, Type=2 };
struct CreatorGroup {std::wstring creator;std::vector<std::size_t> entry_indices;};
[[nodiscard]] std::vector<std::size_t> filter_entries(const std::vector<protocol::caps::InventoryEntry>& entries,std::string_view parent_folder_id,std::wstring_view query,SortMode sort);
[[nodiscard]] std::vector<CreatorGroup> group_entries_by_creator(const std::vector<protocol::caps::InventoryEntry>& entries,const std::vector<std::size_t>& indices);
struct MovePlan {
    std::size_t item_index = 0;
    std::wstring destination; // Proposed creator/store parent folder.
};
[[nodiscard]] std::wstring folder_for(const Item& item);
[[nodiscard]] bool is_demo_item(std::wstring_view name) noexcept;
[[nodiscard]] std::vector<MovePlan> preview_smart_sort(const std::vector<Item>& items);
// No server mutation. User approval, permissions and inventory APIs required
// before any proposed move or deletion may be performed.
[[nodiscard]] std::vector<std::size_t> preview_demo_deletions(
    const std::vector<Item>& items);
} // namespace kami::inventory
