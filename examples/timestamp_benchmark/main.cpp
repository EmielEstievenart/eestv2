#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "eestv/timestamp/timestamp_parser.hpp"

namespace
{

bool apply_parser(const eestv::compiledDataAndTimeParser& parser, std::string& input, eestv::DateAndTime& output, int start_index = 0)
{
    int index = start_index;

    for (const auto& step : parser.dateParser)
    {
        int index_jump = 0;

        if (!step(input, index, index_jump, output))
        {
            return false;
        }

        index += index_jump;
    }

    return true;
}

std::optional<std::size_t> determine_parse_start_slot(const eestv::compiledDataAndTimeParser& parser, std::string& input)
{
    const auto start_indices = eestv::TimestampParser::possible_parse_start_indices(input);

    for (std::size_t candidate_index = 0; candidate_index < start_indices.size(); ++candidate_index)
    {
        eestv::DateAndTime candidate;
        if (apply_parser(parser, input, candidate, start_indices[candidate_index]))
        {
            return candidate_index;
        }
    }

    return std::nullopt;
}

bool detect_timestamp_with_parser_at_known_slot(const eestv::compiledDataAndTimeParser& parser, std::string& input, std::size_t parse_start_slot)
{
    const auto start_indices = eestv::TimestampParser::possible_parse_start_indices(input);
    if (parse_start_slot >= start_indices.size())
    {
        return false;
    }

    eestv::DateAndTime candidate;
    return apply_parser(parser, input, candidate, start_indices[parse_start_slot]);
}

std::vector<std::string> make_log_lines(std::size_t line_count)
{
    std::vector<std::string> lines;
    lines.reserve(line_count);

    for (std::size_t index = 0; index < line_count; ++index)
    {
        std::ostringstream stream;
        stream << "INFO worker[" << (index % 32) << "] " << 2026 << '-' << std::setw(2) << std::setfill('0') << 4 << '-' << std::setw(2) << std::setfill('0') << (1 + (index % 28)) << ' ' << std::setw(2) << std::setfill('0') << (index % 24)
               << ':' << std::setw(2) << std::setfill('0') << ((index / 24) % 60) << ':' << std::setw(2) << std::setfill('0') << ((index / (24 * 60)) % 60) << " finished job " << index;
        lines.push_back(stream.str());
    }

    return lines;
}

template <typename Detector>
std::chrono::steady_clock::duration benchmark_detector(std::vector<std::string>& lines, Detector detector, std::size_t& matches)
{
    matches = 0;

    const auto start_time = std::chrono::steady_clock::now();
    for (auto& line : lines)
    {
        if (detector(line))
        {
            ++matches;
        }
    }

    return std::chrono::steady_clock::now() - start_time;
}

double to_milliseconds(std::chrono::steady_clock::duration duration)
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

std::vector<std::string> make_log_lines_with_timestamp_at_start(std::size_t line_count)
{
    std::vector<std::string> lines;
    lines.reserve(line_count);

    for (std::size_t index = 0; index < line_count; ++index)
    {
        std::ostringstream stream;
        stream << 2026 << '-' << std::setw(2) << std::setfill('0') << 4 << '-' << std::setw(2) << std::setfill('0') << (1 + (index % 28)) << ' ' << std::setw(2) << std::setfill('0') << (index % 24) << ':' << std::setw(2)
               << std::setfill('0') << ((index / 24) % 60) << ':' << std::setw(2) << std::setfill('0') << ((index / (24 * 60)) % 60) << " INFO worker[" << (index % 32) << "] finished job " << index;
        lines.push_back(stream.str());
    }

    return lines;
}

struct BenchmarkResult
{
    std::size_t parse_start_slot = 0;
    std::size_t parser_matches   = 0;
    std::size_t regex_matches    = 0;

    std::chrono::steady_clock::duration parser_duration {};
    std::chrono::steady_clock::duration regex_duration {};
};

BenchmarkResult run_benchmark(const eestv::compiledDataAndTimeParser& parser, const std::regex& timestamp_regex, std::vector<std::string>& lines, std::size_t parse_start_slot)
{
    BenchmarkResult result;
    result.parse_start_slot    = parse_start_slot;
    std::size_t warmup_matches = 0;

    benchmark_detector(lines, [&parser, parse_start_slot](std::string& line) { return detect_timestamp_with_parser_at_known_slot(parser, line, parse_start_slot); }, warmup_matches);
    benchmark_detector(lines, [&timestamp_regex](std::string& line) { return std::regex_search(line.cbegin(), line.cend(), timestamp_regex); }, warmup_matches);

    result.parser_duration = benchmark_detector(lines, [&parser, parse_start_slot](std::string& line) { return detect_timestamp_with_parser_at_known_slot(parser, line, parse_start_slot); }, result.parser_matches);

    result.regex_duration = benchmark_detector(lines, [&timestamp_regex](std::string& line) { return std::regex_search(line.cbegin(), line.cend(), timestamp_regex); }, result.regex_matches);

    return result;
}

void print_benchmark_result(const std::string& title, const std::string& format, std::size_t line_count, const BenchmarkResult& result)
{
    const double parser_ms = to_milliseconds(result.parser_duration);
    const double regex_ms  = to_milliseconds(result.regex_duration);

    std::cout << title << '\n';
    std::cout << "Format: " << format << '\n';
    std::cout << "Lines:  " << line_count << "\n\n";

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Parse start slot:        " << result.parse_start_slot << "\n\n";
    std::cout << "TimestampParser matches: " << result.parser_matches << '\n';
    std::cout << "TimestampParser time:    " << parser_ms << " ms\n\n";

    std::cout << "std::regex matches:     " << result.regex_matches << '\n';
    std::cout << "std::regex time:        " << regex_ms << " ms\n\n";

    if (parser_ms > 0.0)
    {
        std::cout << "regex / parser speedup: " << (regex_ms / parser_ms) << "x\n";
    }

    std::cout << '\n';
}

} // namespace

int main()
{
    constexpr std::size_t line_count = 1000000;
    const std::string format         = "YYYY-MM-DD hh:mm:ss";

    eestv::TimestampParser parser_builder;
    const auto parser = parser_builder.CompileFormat(format);
    const std::regex timestamp_regex(R"(\b\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\b)", std::regex::optimize);

    auto prefixed_lines                  = make_log_lines(line_count);
    const auto prefixed_parse_start_slot = determine_parse_start_slot(parser, prefixed_lines.front());
    if (!prefixed_parse_start_slot.has_value())
    {
        std::cerr << "Failed to determine the parse start slot for prefixed log lines.\n";
        return 1;
    }

    const auto prefixed_result = run_benchmark(parser, timestamp_regex, prefixed_lines, *prefixed_parse_start_slot);
    print_benchmark_result("Timestamp detection benchmark: timestamp after a log prefix", format, line_count, prefixed_result);

    auto timestamp_first_lines                  = make_log_lines_with_timestamp_at_start(line_count);
    const auto timestamp_first_parse_start_slot = determine_parse_start_slot(parser, timestamp_first_lines.front());
    if (!timestamp_first_parse_start_slot.has_value())
    {
        std::cerr << "Failed to determine the parse start slot for timestamp-first log lines.\n";
        return 1;
    }

    const auto timestamp_first_result = run_benchmark(parser, timestamp_regex, timestamp_first_lines, *timestamp_first_parse_start_slot);
    print_benchmark_result("Timestamp detection benchmark: timestamp at the start of the line", format, line_count, timestamp_first_result);

    return 0;
}
