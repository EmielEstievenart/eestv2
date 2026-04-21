#include <gtest/gtest.h>

#include "hard_rules.hpp"
#include "week_pair_generator.hpp"

namespace roster_slayer
{

TEST(WeekPairGeneratorTest, ProducesValidCandidatesOnly)
{
    GeneratorOptions options;
    options.top_n              = 5;
    options.max_expanded_nodes = 200000;

    const GenerationResult result = generate_week_pair_candidates(options);

    EXPECT_FALSE(result.candidates.empty());
    EXPECT_LE(result.candidates.size(), options.top_n);

    for (const GeneratedPairCandidate& candidate : result.candidates)
    {
        const HardValidationResult validation = validate_week_pair(candidate.pair, options.hard_rule_config);
        EXPECT_TRUE(validation.is_valid);
    }
}

TEST(WeekPairGeneratorTest, RespectsTopN)
{
    GeneratorOptions options;
    options.top_n              = 3;
    options.max_expanded_nodes = 300000;

    const GenerationResult result = generate_week_pair_candidates(options);

    EXPECT_LE(result.candidates.size(), 3U);
}

} // namespace roster_slayer
