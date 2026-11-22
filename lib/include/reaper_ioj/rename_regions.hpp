#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace ioj {

struct MediaItem {
    double start;
    double end;
    std::string take_name;
};

struct Region {
    int number;
    double start;
    double end;
    std::string name;
};

struct Rename {
    std::size_t region_index;
    std::string name;

    auto operator==(Rename const&) const -> bool = default;
};

using RenamePlan = std::expected<std::vector<Rename>, std::string>;

auto plan_region_names(std::span<MediaItem const> items, std::span<Region const> regions) -> RenamePlan;

}
