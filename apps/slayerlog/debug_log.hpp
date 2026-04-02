#pragma once

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>

#include <filesystem>
#include <ios>
#include <mutex>
#include <sstream>
#include <string>

namespace slayerlog
{
namespace debug_log
{

inline const char* get_filename(const char* path)
{
    const char* filename = path;
    for (const char* current = path; *current != '\0'; ++current)
    {
        if (*current == '/' || *current == '\\')
        {
            filename = current + 1;
        }
    }

    return filename;
}

inline std::filesystem::path compute_log_file_path()
{
    try
    {
        return std::filesystem::current_path() / "slayerlog_debug.log";
    }
    catch (...)
    {
        return "slayerlog_debug.log";
    }
}

inline const std::filesystem::path& log_file_path()
{
    static const std::filesystem::path path = compute_log_file_path();
    return path;
}

inline boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>& logger()
{
    static boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> instance;
    return instance;
}

inline void initialize()
{
    static std::once_flag initialized;
    std::call_once(
        initialized,
        []
        {
            namespace expr     = boost::log::expressions;
            namespace keywords = boost::log::keywords;

            const auto severity = expr::attr<boost::log::trivial::severity_level>("Severity");

            boost::log::add_file_log(
                keywords::file_name = log_file_path().string(),
                keywords::open_mode = std::ios_base::app,
                keywords::auto_flush = true,
                keywords::format =
                    expr::stream << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << "] "
                                 << "[" << severity << "] "
                                 << "[" << expr::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "] "
                                 << expr::smessage);

            boost::log::add_common_attributes();
            boost::log::core::get()->set_filter(severity >= boost::log::trivial::trace);
        });
}

inline void write(
    boost::log::trivial::severity_level severity,
    const char* file,
    int line,
    const char* function_name,
    const std::string& message)
{
    initialize();
    BOOST_LOG_SEV(logger(), severity) << get_filename(file) << ":" << line << " [" << function_name << "] " << message;
}

} // namespace debug_log
} // namespace slayerlog

#define SLAYERLOG_LOG_ERROR(message)                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        std::ostringstream slayerlog_debug_stream__;                                                                               \
        slayerlog_debug_stream__ << message;                                                                                       \
        ::slayerlog::debug_log::write(                                                                                             \
            ::boost::log::trivial::error, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str());                          \
    } while (0)

#define SLAYERLOG_LOG_WARNING(message)                                                                                             \
    do                                                                                                                             \
    {                                                                                                                              \
        std::ostringstream slayerlog_debug_stream__;                                                                               \
        slayerlog_debug_stream__ << message;                                                                                       \
        ::slayerlog::debug_log::write(                                                                                             \
            ::boost::log::trivial::warning, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str());                        \
    } while (0)

#define SLAYERLOG_LOG_INFO(message)                                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        std::ostringstream slayerlog_debug_stream__;                                                                               \
        slayerlog_debug_stream__ << message;                                                                                       \
        ::slayerlog::debug_log::write(                                                                                             \
            ::boost::log::trivial::info, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str());                           \
    } while (0)

#define SLAYERLOG_LOG_DEBUG(message)                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        std::ostringstream slayerlog_debug_stream__;                                                                               \
        slayerlog_debug_stream__ << message;                                                                                       \
        ::slayerlog::debug_log::write(                                                                                             \
            ::boost::log::trivial::debug, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str());                          \
    } while (0)

#define SLAYERLOG_LOG_TRACE(message)                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        std::ostringstream slayerlog_debug_stream__;                                                                               \
        slayerlog_debug_stream__ << message;                                                                                       \
        ::slayerlog::debug_log::write(                                                                                             \
            ::boost::log::trivial::trace, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str());                          \
    } while (0)
