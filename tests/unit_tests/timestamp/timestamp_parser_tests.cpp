#include <gtest/gtest.h>

#include "eestv/timestamp/timestamp_parser.hpp"

namespace eestv
{

TEST(TimestampParserTest, ParsesTwoDigitYearToken)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YY");

    ASSERT_EQ(compiled.dateParser.size(), 1U);

    std::string input = "26";
    DateAndTime output;
    int index_jump = 0;

    EXPECT_TRUE(compiled.dateParser.front()(input, 0, index_jump, output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(index_jump, 2);
}

TEST(TimestampParserTest, ParsesFourDigitYearToken)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY");

    ASSERT_EQ(compiled.dateParser.size(), 1U);

    std::string input = "2026";
    DateAndTime output;
    int index_jump = 0;

    EXPECT_TRUE(compiled.dateParser.front()(input, 0, index_jump, output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(index_jump, 4);
}

} // namespace eestv
