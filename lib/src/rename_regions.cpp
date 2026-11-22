#include "reaper_ioj/rename_regions.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <map>
#include <numeric>
#include <string_view>
#include <unordered_map>

namespace ioj {
namespace {

template<typename T>
auto indices_by_start(std::span<T const> entries) -> std::vector<std::size_t>
{
    std::vector<std::size_t> indices{};
    indices.resize(entries.size());
    std::iota(indices.begin(), indices.end(), std::size_t{0});
    std::stable_sort(indices.begin(), indices.end(), [&](auto left, auto right) -> bool {
        return entries[left].start < entries[right].start;
    });
    return indices;
}

auto ambiguous_parent_error(Region const& region, std::span<MediaItem const> items) -> RenamePlan
{
    RenamePlan result{std::unexpect};
    auto out{std::back_inserter(result.error())};
    std::format_to(out, "Region {} has multiple enclosing media items.", region.number);

    std::size_t match_index{};
    for (auto const& item : items) {
        if (item.start <= region.start && item.end >= region.end) {
            std::string_view const name{item.take_name};
            std::format_to(out, "\n  - {}: {}", match_index++, name.empty() ? "(unnamed)" : name);
        }
    }
    return result;
}

}

auto plan_region_names(std::span<MediaItem const> items, std::span<Region const> regions) -> RenamePlan
{
    auto const item_order{indices_by_start(items)};
    auto const region_order{indices_by_start(regions)};
    std::size_t next_item{};
    std::multimap<double, std::size_t> started_items_by_end{};
    std::vector<std::size_t> counters{};
    counters.resize(items.size());
    std::unordered_map<std::string, int> assigned_names{};
    RenamePlan changes{std::in_place};
    std::string name{};

    for (std::size_t i{}; i < region_order.size(); ++i) {
        auto const index{region_order[i]};
        auto const& region{regions[index]};
        if (i > 0 && regions[region_order[i - 1]].start == region.start) {
            return RenamePlan{std::unexpect, std::format("Region {} has the same start time as region {}.",
                region.number, regions[region_order[i - 1]].number)};
        }

        while (next_item < item_order.size() && items[item_order[next_item]].start <= region.start) {
            auto const item_index{item_order[next_item++]};
            started_items_by_end.emplace(items[item_index].end, item_index);
        }
        auto const parent{started_items_by_end.lower_bound(region.end)};
        if (parent == started_items_by_end.end()) {
            return RenamePlan{std::unexpect, std::format("Region {} has no enclosing media item.", region.number)};
        }
        if (std::next(parent) != started_items_by_end.end()) {
            return ambiguous_parent_error(region, items);
        }

        auto const item_index{parent->second};
        auto const& take_name{items[item_index].take_name};
        if (take_name.empty()) {
            return RenamePlan{std::unexpect, std::format(
                "Region {} has an enclosing media item without a named active take.", region.number)};
        }

        name.clear();
        std::format_to(std::back_inserter(name), "{}_{}", take_name, counters[item_index]++);
        auto const [entry, inserted]{assigned_names.try_emplace(name, region.number)};
        if (!inserted) {
            return RenamePlan{std::unexpect, std::format(
                "Region {} and region {} would both be named '{}'.", region.number, entry->second, name)};
        }
        if (name != region.name) {
            changes->emplace_back(index, name);
        }
    }
    return changes;
}

}
