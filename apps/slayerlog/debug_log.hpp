#pragma once

#include <boost/log/trivial.hpp>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace slayerlog
{
namespace debug_log
{

inline constexpr std::size_t max_log_file_size_bytes = 4u * 1024u * 1024u;

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

inline std::filesystem::path log_directory_path()
{
    const auto& path = log_file_path();
    return path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
}

inline std::filesystem::path rollover_log_file_path()
{
    return log_directory_path() / "slayerlog_debug_1.log";
}

inline std::mutex& log_mutex()
{
    static std::mutex instance;
    return instance;
}

inline const char* severity_name(boost::log::trivial::severity_level severity)
{
    switch (severity)
    {
    case boost::log::trivial::trace:
        return "trace";
    case boost::log::trivial::debug:
        return "debug";
    case boost::log::trivial::info:
        return "info";
    case boost::log::trivial::warning:
        return "warning";
    case boost::log::trivial::error:
        return "error";
    case boost::log::trivial::fatal:
        return "fatal";
    default:
        return "unknown";
    }
}

inline void prune_old_rollover_files()
{
    std::error_code error;
    const auto directory = log_directory_path();
    std::filesystem::create_directories(directory, error);
    error.clear();

    const auto keep_path = rollover_log_file_path().filename().string();
    for (const auto& entry : std::filesystem::directory_iterator(directory, error))
    {
        if (error)
        {
            return;
        }

        if (!entry.is_regular_file(error) || error)
        {
            error.clear();
            continue;
        }

        const auto filename = entry.path().filename().string();
        if (filename == keep_path)
        {
            continue;
        }

        if (filename.rfind("slayerlog_debug_", 0) == 0 && entry.path().extension() == ".log")
        {
            std::filesystem::remove(entry.path(), error);
            error.clear();
        }
    }
}

inline void rotate_log_file_if_needed(std::size_t bytes_to_write)
{
    std::error_code error;
    const auto active_path   = log_file_path();
    const auto rollover_path = rollover_log_file_path();

    const auto current_size = std::filesystem::exists(active_path, error) ? std::filesystem::file_size(active_path, error) : 0;
    if (error || current_size + bytes_to_write <= max_log_file_size_bytes)
    {
        return;
    }

    std::filesystem::remove(rollover_path, error);
    error.clear();

    std::filesystem::rename(active_path, rollover_path, error);
    if (!error)
    {
        return;
    }

    error.clear();
    std::filesystem::copy_file(active_path, rollover_path, std::filesystem::copy_options::overwrite_existing, error);
    if (error)
    {
        return;
    }

    error.clear();
    std::ofstream truncate_output(active_path, std::ios::trunc);
}

inline std::string format_timestamp()
{
    const auto now    = std::chrono::system_clock::now();
    const auto time   = std::chrono::system_clock::to_time_t(now);
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % std::chrono::seconds(1);

    std::tm local_time {};
#ifdef _WIN32
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << "." << std::setw(6) << std::setfill('0') << micros.count();
    return output.str();
}

inline std::string format_thread_id()
{
    std::ostringstream output;
    output << std::this_thread::get_id();
    return output.str();
}

inline void initialize()
{
    static std::once_flag initialized;
    std::call_once(initialized,
                   []
                   {
                       std::error_code error;
                       std::filesystem::create_directories(log_directory_path(), error);
                       prune_old_rollover_files();
                   });
}

inline void write(boost::log::trivial::severity_level severity, const char* file, int line, const char* function_name,
                  const std::string& message)
{
    initialize();

    std::lock_guard<std::mutex> lock(log_mutex());

    std::ostringstream record;
    record << "[" << format_timestamp() << "] "
           << "[" << severity_name(severity) << "] "
           << "[" << format_thread_id() << "] " << get_filename(file) << ":" << line << " [" << function_name << "] " << message << "\n";

    const auto text = record.str();
    rotate_log_file_if_needed(text.size());

    std::ofstream output(log_file_path(), std::ios::app);
    if (!output.is_open())
    {
        return;
    }

    output << text;
    output.flush();
}

} // namespace debug_log
} // namespace slayerlog

#define SLAYERLOG_LOG_ERROR(message)                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        std::ostringstream slayerlog_debug_stream__;                                                                               \
        slayerlog_debug_stream__ << message;                                                                                       \
        ::slayerlog::debug_log::write(::boost::log::trivial::error, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str()); \
    } while (0)

#define SLAYERLOG_LOG_WARNING(message)                                                                                               \
    do                                                                                                                               \
    {                                                                                                                                \
        std::ostringstream slayerlog_debug_stream__;                                                                                 \
        slayerlog_debug_stream__ << message;                                                                                         \
        ::slayerlog::debug_log::write(::boost::log::trivial::warning, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str()); \
    } while (0)

#define SLAYERLOG_LOG_INFO(message)                                                                                               \
    do                                                                                                                            \
    {                                                                                                                             \
        std::ostringstream slayerlog_debug_stream__;                                                                              \
        slayerlog_debug_stream__ << message;                                                                                      \
        ::slayerlog::debug_log::write(::boost::log::trivial::info, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str()); \
    } while (0)

#define SLAYERLOG_LOG_DEBUG(message)                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        std::ostringstream slayerlog_debug_stream__;                                                                               \
        slayerlog_debug_stream__ << message;                                                                                       \
        ::slayerlog::debug_log::write(::boost::log::trivial::debug, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str()); \
    } while (0)

#define SLAYERLOG_LOG_TRACE(message)                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        std::ostringstream slayerlog_debug_stream__;                                                                               \
        slayerlog_debug_stream__ << message;                                                                                       \
        ::slayerlog::debug_log::write(::boost::log::trivial::trace, __FILE__, __LINE__, __func__, slayerlog_debug_stream__.str()); \
    } while (0)
