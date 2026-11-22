#include "rename_regions_action.hpp"

#include "reaper_api.hpp"
#include "reaper_ioj/rename_regions.hpp"

#include <format>
#include <ranges>

namespace ioj {
namespace {

class UndoBlock {
public:
    UndoBlock(ReaProject* project, char const* description, int flags)
        : project_{project}, description_{description}, flags_{flags}
    {
        Undo_BeginBlock2(project_);
    }

    ~UndoBlock()
    {
        Undo_EndBlock2(project_, description_, flags_);
    }

    UndoBlock(UndoBlock const&) = delete;
    auto operator=(UndoBlock const&) -> UndoBlock& = delete;

private:
    ReaProject* project_;
    char const* description_;
    int flags_;
};

struct ProjectRegions {
    std::vector<Region> regions{};
    std::vector<ProjectMarker*> handles{};
};

auto read_items(ReaProject* project) -> std::vector<MediaItem>
{
    std::vector<MediaItem> items{};
    auto const track_count{CountTracks(project)};

    for (int track_index{}; track_index < track_count; ++track_index) {
        auto* track{GetTrack(project, track_index)};
        auto const item_count{CountTrackMediaItems(track)};

        for (int i{}; i < item_count; ++i) {
            auto* item{GetTrackMediaItem(track, i)};
            auto const start{GetMediaItemInfo_Value(item, "D_POSITION")};
            auto const length{GetMediaItemInfo_Value(item, "D_LENGTH")};

            auto* take{GetActiveTake(item)};
            auto const* name{take ? static_cast<char const*>(GetSetMediaItemTakeInfo(take, "P_NAME", nullptr)) : nullptr};
            items.push_back({start, start + length, name ? name : ""});
        }
    }

    return items;
}

auto read_regions(ReaProject* project) -> ProjectRegions
{
    ProjectRegions result{};
    auto const count{GetNumRegionsOrMarkers(project)};

    for (int i{}; i < count; ++i) {
        auto* marker{GetRegionOrMarker(project, i, nullptr)};
        if (!GetRegionOrMarkerInfo_Value(project, marker, "B_ISREGION")) {
            continue;
        }

        auto const* name{static_cast<char const*>(GetSetRegionOrMarkerInfo(project, marker, "P_NAME", nullptr))};
        result.regions.push_back({
            static_cast<int>(GetRegionOrMarkerInfo_Value(project, marker, "I_NUMBER")),
            GetRegionOrMarkerInfo_Value(project, marker, "D_STARTPOS"),
            GetRegionOrMarkerInfo_Value(project, marker, "D_ENDPOS"),
            name ? name : ""
        });
        result.handles.push_back(marker);
    }

    return result;
}

auto log_error(std::string const& message) -> void
{
    ShowConsoleMsg(std::format("ioj: {}\n", message).c_str());
}

auto apply_region_names(ReaProject* project, ProjectRegions& snapshot, std::span<Rename> changes) -> void
{
    auto const set_name{[&](std::size_t index, std::string& name) -> bool {
        return GetSetRegionOrMarkerInfo_String(project, snapshot.handles[index], "P_NAME", name.data(), true);
    }};

    UndoBlock const undo{project, rename_regions_action_name, UNDO_STATE_MISCCFG};
    auto const count{changes.size()};

    for (std::size_t i{}; i < count; ++i) {
        auto& change{changes[i]};
        if (set_name(change.region_index, change.name)) {
            continue;
        }

        bool restored{true};
        for (auto const& applied : changes.first(i) | std::views::reverse) {
            auto const index{applied.region_index};
            if (!set_name(index, snapshot.regions[index].name)) {
                restored = false;
            }
        }

        log_error(std::format("REAPER could not rename region {}.{}", snapshot.regions[change.region_index].number,
            restored ? "" : " Use Undo to restore the previous names."));
        return;
    }
}

}

auto rename_regions() -> void
{
    auto* project{EnumProjects(-1, nullptr, 0)};
    auto snapshot{read_regions(project)};
    if (snapshot.regions.empty()) {
        return;
    }

    auto plan{plan_region_names(read_items(project), snapshot.regions)};
    if (!plan) {
        log_error(plan.error());
        return;
    }

    if (!plan->empty()) {
        apply_region_names(project, snapshot, *plan);
        UpdateArrange();
    }
}

}
