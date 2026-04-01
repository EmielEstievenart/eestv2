#include "file_watcher.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace slayerlog
{

std::vector<std::string> FileWatcher::parse_lines_from_chunk(std::string chunk, FileWatcher::State& state)
{
    std::vector<std::string> lines;
    if (!state.pending_fragment.empty())
    {
        chunk.insert(0, state.pending_fragment);
        state.pending_fragment.clear();
    }

    std::size_t start = 0;
    while (start < chunk.size())
    {
        const auto newline = chunk.find('\n', start);
        if (newline == std::string::npos)
        {
            state.pending_fragment = chunk.substr(start);
            break;
        }

        auto line = chunk.substr(start, newline - start);
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        lines.push_back(std::move(line));
        start = newline + 1;
    }

    return lines;
}

std::vector<std::string> FileWatcher::read_file_lines(const std::string& path, FileWatcher::State& state)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse_lines_from_chunk(buffer.str(), state);
}

std::string FileWatcher::read_file_tail(const std::string& path, std::uintmax_t offset)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    input.seekg(0, std::ios::end);
    const auto size_position = input.tellg();
    if (size_position < 0)
    {
        throw std::runtime_error("Failed to determine file size: " + path);
    }

    const auto file_size = static_cast<std::uintmax_t>(size_position);
    if (offset >= file_size)
    {
        return {};
    }

    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    std::string chunk(file_size - offset, '\0');
    input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    chunk.resize(static_cast<std::size_t>(input.gcount()));
    return chunk;
}

std::uintmax_t FileWatcher::get_file_size(const std::string& path)
{
    return std::filesystem::file_size(std::filesystem::path(path));
}

FileWatcher::FileWatcher(std::string file_path, FileWatcher::Callback callback)
    : _file_path(std::move(file_path)),
      _callback(std::move(callback))
{
    State state;
    auto lines = read_file_lines(_file_path, state);
    state.offset = get_file_size(_file_path);

    {
        std::lock_guard lock(_mutex);
        _state = std::move(state);
    }

    Update update;
    update.kind = Update::Kind::Snapshot;
    update.lines = std::move(lines);
    notify(std::move(update));
}

void FileWatcher::process_once()
{
    Update update;
    bool has_update = false;
    {
        std::lock_guard lock(_mutex);

        const auto file_size = get_file_size(_file_path);
        if (file_size < _state.offset)
        {
            _state = State{};
            auto lines = read_file_lines(_file_path, _state);
            _state.offset = file_size;
            update.kind = Update::Kind::Snapshot;
            update.lines = std::move(lines);
            has_update = true;
        }

        if (!has_update && file_size == _state.offset)
        {
            return;
        }

        if (!has_update)
        {
            auto chunk = read_file_tail(_file_path, _state.offset);
            _state.offset = file_size;
            if (!chunk.empty())
            {
                auto lines = parse_lines_from_chunk(std::move(chunk), _state);
                if (!lines.empty())
                {
                    update.kind = Update::Kind::Append;
                    update.lines = std::move(lines);
                    has_update = true;
                }
            }
        }
    }

    if (has_update)
    {
        notify(std::move(update));
    }
}

void FileWatcher::notify(Update update)
{
    Callback callback;
    {
        std::lock_guard lock(_mutex);
        callback = _callback;
    }

    if (callback)
    {
        callback(update);
    }
}

} // namespace slayerlog
