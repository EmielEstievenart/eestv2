#include <gtest/gtest.h>

#include "soft_scoring.hpp"

namespace roster_slayer
{

TEST(SoftScoringTest, PenalizesSplitWeekendShifts)
{
    WeekPairPlanning no_split;
    no_split.worked_week.days = {
        AssignmentCode::Ea,
        AssignmentCode::Ea,
        AssignmentCode::Off,
        AssignmentCode::D,
        AssignmentCode::L1,
        AssignmentCode::WeEa,
        AssignmentCode::WeL,
    };
    no_split.off_week.days = {
        AssignmentCode::Off,
        AssignmentCode::Ea,
        AssignmentCode::Ea,
        AssignmentCode::D,
        AssignmentCode::L1,
        AssignmentCode::Off,
        AssignmentCode::Off,
    };

    WeekPairPlanning with_split = no_split;
    with_split.worked_week.days[5] = AssignmentCode::WeSd;
    with_split.worked_week.days[6] = AssignmentCode::WeSl;

    const SoftScoreBreakdown no_split_score   = evaluate_soft_score(no_split);
    const SoftScoreBreakdown with_split_score = evaluate_soft_score(with_split);

    EXPECT_GT(no_split_score.total(), with_split_score.total());
}

TEST(SoftScoringTest, PrefersCoherentSequencesOverJaggedPattern)
{
    WeekPairPlanning coherent;
    coherent.worked_week.days = {
        AssignmentCode::Ea,
        AssignmentCode::Ea,
        AssignmentCode::Off,
        AssignmentCode::D,
        AssignmentCode::L1,
        AssignmentCode::WeEa,
        AssignmentCode::WeL,
    };
    coherent.off_week.days = {
        AssignmentCode::Off,
        AssignmentCode::Ea,
        AssignmentCode::Ea,
        AssignmentCode::D,
        AssignmentCode::L1,
        AssignmentCode::Off,
        AssignmentCode::Off,
    };

    WeekPairPlanning jagged = coherent;
    jagged.worked_week.days = {
        AssignmentCode::Ea,
        AssignmentCode::L1,
        AssignmentCode::Off,
        AssignmentCode::Eb,
        AssignmentCode::D,
        AssignmentCode::WeSl,
        AssignmentCode::WeEa,
    };

    const SoftScoreBreakdown coherent_score = evaluate_soft_score(coherent);
    const SoftScoreBreakdown jagged_score   = evaluate_soft_score(jagged);

    EXPECT_GT(coherent_score.total(), jagged_score.total());
}

} // namespace roster_slayer
