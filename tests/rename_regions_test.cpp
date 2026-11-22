#include "reaper_ioj/rename_regions.hpp"

#include <gtest/gtest.h>

namespace ioj {
namespace {

TEST(RegionNames, OrdersByStartAndRestartsForEachParent)
{
    std::vector<MediaItem> const items{{10, 15, "bar.wav"}, {0, 10, "foo"}};
    std::vector<Region> const regions{
        {20, 4, 5, "old"}, {8, 10, 15, ""}, {90, 0, 2, "old"}, {1, 5, 10, ""}
    };
    std::vector<Rename> const expected{{2, "foo_0"}, {0, "foo_1"}, {3, "foo_2"}, {1, "bar.wav_0"}};
    EXPECT_EQ(plan_region_names(items, regions), expected);
}

TEST(RegionNames, AlreadyCorrectNamesStillConsumeTheirIndex)
{
    std::vector<MediaItem> const items{{0, 8, "foo"}};
    std::vector<Region> const regions{{1, 1, 2, "foo_0"}, {2, 2, 3, ""}};
    std::vector<Rename> const expected{{1, "foo_1"}};
    EXPECT_EQ(plan_region_names(items, regions), expected);
}

TEST(RegionNames, EmptyProjectAndRerunProduceNoEdits)
{
    std::vector<MediaItem> const items{{0, 1, "foo"}};
    std::vector<Region> const regions{{1, 0, 1, "foo_0"}};
    RenamePlan const expected{std::in_place};
    EXPECT_EQ(plan_region_names({}, {}), expected);
    EXPECT_EQ(plan_region_names(items, regions), expected);
}

TEST(RegionNames, EqualStartsReportBothRegionsEvenWithDifferentEnds)
{
    std::vector<MediaItem> const items{{0, 4, "foo"}};
    std::vector<Region> const regions{{3, 1, 2, ""}, {8, 1, 4, ""}};
    std::unexpected<std::string> const error{std::in_place, "Region 8 has the same start time as region 3."};
    EXPECT_EQ(plan_region_names(items, regions), error);
}

TEST(RegionNames, LaterRegionCrossingItemBoundaryInvalidatesEntirePlan)
{
    std::vector<MediaItem> const items{{0, 4, "foo"}, {4, 8, "bar"}};
    std::vector<Region> const regions{{1, 0, 1, ""}, {2, 3, 5, ""}};
    std::unexpected<std::string> const error{std::in_place, "Region 2 has no enclosing media item."};
    EXPECT_EQ(plan_region_names(items, regions), error);
}

TEST(RegionNames, ListsEveryEnclosingItemInProjectOrder)
{
    std::vector<MediaItem> const items{
        {0, 1, "ends early"}, {0, 5, "Foo"}, {2, 6, "starts late"}, {1, 3, "Bar"}, {0, 4, ""}
    };
    std::vector<Region> const regions{{1, 1, 3, ""}};
    std::unexpected<std::string> const error{std::in_place,
        "Region 1 has multiple enclosing media items.\n  - 0: Foo\n  - 1: Bar\n  - 2: (unnamed)"};
    EXPECT_EQ(plan_region_names(items, regions), error);
}

TEST(RegionNames, ParentNeedsANamedActiveTake)
{
    std::vector<MediaItem> const items{{0, 4, ""}};
    std::vector<Region> const regions{{3, 1, 2, ""}};
    std::unexpected<std::string> const error{std::in_place, "Region 3 has an enclosing media item without a named active take."};
    EXPECT_EQ(plan_region_names(items, regions), error);
}

TEST(RegionNames, DuplicateTargetsFailEvenWhenOneNameIsAlreadyCorrect)
{
    std::vector<MediaItem> const items{{0, 4, "foo"}, {5, 8, "foo"}};
    std::vector<Region> const regions{{10, 0, 2, "foo_0"}, {20, 5, 7, ""}};
    std::unexpected<std::string> const error{std::in_place, "Region 20 and region 10 would both be named 'foo_0'."};
    EXPECT_EQ(plan_region_names(items, regions), error);
}

}
}
