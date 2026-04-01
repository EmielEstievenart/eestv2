#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace slayerlog
{

class FileWatcher
{
public:
    struct Update
    {
        enum class Kind
        {
            Snapshot,
            Append,
        };

        Kind kind = Kind::Snapshot;
        std::vector<std::string> lines;
    };

    using Callback = std::function<void(const Update&)>;

    FileWatcher(std::string file_path, Callback callback);

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    void process_once();

private:
    struct State
    {
        std::uintmax_t offset = 0;
        std::string pending_fragment;
    };

    static std::vector<std::string> parse_lines_from_chunk(std::string chunk, State& state);
    static std::vector<std::string> read_file_lines(const std::string& path, State& state);
    static std::string read_file_tail(const std::string& path, std::uintmax_t offset);
    static std::uintmax_t get_file_size(const std::string& path);

    void notify(Update update);

    std::string _file_path;
    State _state;
    Callback _callback;
    std::mutex _mutex;
};

} // namespace slayerlog
