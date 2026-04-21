#include <gtest/gtest.h>

#include "hard_rules.hpp"

namespace roster_slayer
{

namespace
{

WeekPairPlanning make_valid_pair()
{
    WeekPairPlanning pair;

    pair.worked_week.days = {
        AssignmentCode::Ea,
        AssignmentCode::Ea,
        AssignmentCode::Off,
        AssignmentCode::D,
        AssignmentCode::L1,
        AssignmentCode::WeL,
        AssignmentCode::WeL,
    };

    pair.off_week.days = {
        AssignmentCode::Off,
        AssignmentCode::Ea,
        AssignmentCode::Ea,
        AssignmentCode::D,
        AssignmentCode::L1,
        AssignmentCode::Off,
        AssignmentCode::Off,
    };

    return pair;
}

} // namespace

TEST(HardRulesTest, AcceptsValidWorkedWeekToOffWeekPair)
{
    const WeekPairPlanning pair = make_valid_pair();
    const auto validation       = validate_week_pair(pair, HardRuleConfig {});

    EXPECT_TRUE(validation.is_valid);
    EXPECT_TRUE(validation.errors.empty());
}

TEST(HardRulesTest, RejectsLateToEarlyTransition)
{
    WeekPairPlanning pair = make_valid_pair();
    pair.worked_week.days[1] = AssignmentCode::L1;
    pair.worked_week.days[2] = AssignmentCode::Ea;

    const auto validation = validate_week_pair(pair, HardRuleConfig {});

    EXPECT_FALSE(validation.is_valid);
}

TEST(HardRulesTest, RejectsMoreThanFourConsecutiveWorkdays)
{
    WeekPairPlanning pair = make_valid_pair();
    pair.worked_week.days = {
        AssignmentCode::Ea,
        AssignmentCode::Ea,
        AssignmentCode::D,
        AssignmentCode::L1,
        AssignmentCode::L2,
        AssignmentCode::WeEa,
        AssignmentCode::WeL,
    };

    const auto validation = validate_week_pair(pair, HardRuleConfig {});

    EXPECT_FALSE(validation.is_valid);
}

TEST(HardRulesTest, RequiresDayOffBeforeAndAfterWorkedWeekend)
{
    WeekPairPlanning pair = make_valid_pair();

    pair.worked_week.days[2] = AssignmentCode::Ea;
    pair.off_week.days[0]    = AssignmentCode::Ea;
    pair.off_week.days[4]    = AssignmentCode::Ea;

    const auto validation = validate_week_pair(pair, HardRuleConfig {});

    EXPECT_FALSE(validation.is_valid);
}

} // namespace roster_slayer
