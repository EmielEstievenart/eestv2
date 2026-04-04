#include "input_controller.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#endif

namespace slayerlog
{

namespace
{

#ifndef _WIN32

bool env_var_is_set(const char* name)
{
    const auto* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

bool is_ssh_session()
{
    return env_var_is_set("SSH_CONNECTION") || env_var_is_set("SSH_CLIENT") || env_var_is_set("SSH_TTY");
}

bool write_text_to_command(const char* command, const std::string& text)
{
    auto* pipe = popen(command, "w");
    if (pipe == nullptr)
    {
        return false;
    }

    const std::size_t bytes_written = std::fwrite(text.data(), 1, text.size(), pipe);
    const int status                = pclose(pipe);
    return bytes_written == text.size() && status == 0;
}

std::string base64_encode(const std::string& text)
{
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((text.size() + 2) / 3) * 4);

    std::size_t index = 0;
    while (index + 3 <= text.size())
    {
        const unsigned int value = (static_cast<unsigned char>(text[index]) << 16) | (static_cast<unsigned char>(text[index + 1]) << 8) |
                                   static_cast<unsigned char>(text[index + 2]);
        encoded.push_back(alphabet[(value >> 18) & 0x3F]);
        encoded.push_back(alphabet[(value >> 12) & 0x3F]);
        encoded.push_back(alphabet[(value >> 6) & 0x3F]);
        encoded.push_back(alphabet[value & 0x3F]);
        index += 3;
    }

    const std::size_t remainder = text.size() - index;
    if (remainder == 1)
    {
        const unsigned int value = static_cast<unsigned char>(text[index]) << 16;
        encoded.push_back(alphabet[(value >> 18) & 0x3F]);
        encoded.push_back(alphabet[(value >> 12) & 0x3F]);
        encoded.push_back('=');
        encoded.push_back('=');
    }
    else if (remainder == 2)
    {
        const unsigned int value = (static_cast<unsigned char>(text[index]) << 16) | (static_cast<unsigned char>(text[index + 1]) << 8);
        encoded.push_back(alphabet[(value >> 18) & 0x3F]);
        encoded.push_back(alphabet[(value >> 12) & 0x3F]);
        encoded.push_back(alphabet[(value >> 6) & 0x3F]);
        encoded.push_back('=');
    }

    return encoded;
}

std::string build_osc52_sequence(const std::string& text)
{
    const std::string payload = "52;c;" + base64_encode(text) + '\a';

    if (env_var_is_set("TMUX"))
    {
        return "\x1bPtmux;\x1b]" + payload + "\x1b\\";
    }

    const auto* term = std::getenv("TERM");
    if (term != nullptr && std::string(term).rfind("screen", 0) == 0)
    {
        return "\x1bP\x1b]" + payload + "\x1b\\";
    }

    return "\x1b]" + payload;
}

bool write_text_to_terminal_clipboard(const std::string& text)
{
    auto* terminal = std::fopen("/dev/tty", "w");
    if (terminal == nullptr)
    {
        return false;
    }

    const auto sequence      = build_osc52_sequence(text);
    const auto bytes_written = std::fwrite(sequence.data(), 1, sequence.size(), terminal);
    const bool flushed       = std::fflush(terminal) == 0;
    std::fclose(terminal);

    return bytes_written == sequence.size() && flushed;
}

bool copy_with_local_clipboard_tools(const std::string& text)
{
#    ifdef __APPLE__
    if (write_text_to_command("pbcopy 2>/dev/null", text))
    {
        return true;
    }
#    endif

    if (write_text_to_command("wl-copy --type text/plain;charset=utf-8 2>/dev/null", text))
    {
        return true;
    }

    if (write_text_to_command("xclip -in -selection clipboard 2>/dev/null", text))
    {
        return true;
    }

    if (write_text_to_command("xsel --clipboard --input 2>/dev/null", text))
    {
        return true;
    }

    return false;
}

bool copy_text_to_clipboard_on_unix(const std::string& text)
{
    // Over SSH we prefer OSC52 first because desktop clipboard commands would
    // target the remote machine, not the local terminal session.
    if (is_ssh_session())
    {
        return write_text_to_terminal_clipboard(text) || copy_with_local_clipboard_tools(text);
    }

    // For local desktop sessions prefer native clipboard tools first, then
    // fall back to OSC52 for terminals that support clipboard escape sequences.
    return copy_with_local_clipboard_tools(text) || write_text_to_terminal_clipboard(text);
}

#endif

} // namespace

InputController::InputController(LogViewModel& model, LogView& view, ftxui::ScreenInteractive& screen,
                                 CommandPaletteController& command_palette_controller)
    : _model(model), _view(view), _screen(screen), _command_palette_controller(command_palette_controller)
{
}

bool InputController::handle_event(ftxui::Event event)
{
    if (event == ftxui::Event::CtrlP)
    {
        _command_palette_controller.open();
        return true;
    }

    if (_command_palette_controller.is_open())
    {
        return _command_palette_controller.handle_event(event);
    }

    if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape)
    {
        _screen.Exit();
        return true;
    }

    if (event == ftxui::Event::Custom)
    {
        return true;
    }

    if (event == ftxui::Event::Character('p'))
    {
        _model.toggle_pause();
        return true;
    }

    if (event == ftxui::Event::C)
    {
        return copy_selection_to_clipboard();
    }

    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k'))
    {
        _model.scroll_up();
        return true;
    }

    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j'))
    {
        _model.scroll_down();
        return true;
    }

    if (event == ftxui::Event::PageUp)
    {
        _model.scroll_up(_model.visible_line_count());
        return true;
    }

    if (event == ftxui::Event::PageDown)
    {
        _model.scroll_down(_model.visible_line_count());
        return true;
    }

    if (event == ftxui::Event::Home)
    {
        _model.scroll_to_top();
        return true;
    }

    if (event == ftxui::Event::End)
    {
        _model.scroll_to_bottom();
        return true;
    }

    if (event.is_mouse())
    {
        if (event.mouse().button == ftxui::Mouse::Left)
        {
            const auto position = _view.mouse_to_text_position(_model, event.mouse());
            if (event.mouse().motion == ftxui::Mouse::Pressed)
            {
                if (position.has_value())
                {
                    _model.begin_selection(*position);
                    return true;
                }

                _model.clear_selection();
                return false;
            }

            if (event.mouse().motion == ftxui::Mouse::Moved && _model.selection_in_progress())
            {
                if (position.has_value())
                {
                    _model.update_selection(*position);
                    return true;
                }
            }

            if (event.mouse().motion == ftxui::Mouse::Released)
            {
                _model.end_selection(position);
                return position.has_value();
            }
        }

        if (event.mouse().button == ftxui::Mouse::Right && event.mouse().motion == ftxui::Mouse::Pressed)
        {
            return copy_selection_to_clipboard();
        }

        if (event.mouse().button == ftxui::Mouse::WheelUp)
        {
            _model.scroll_up();
            return true;
        }

        if (event.mouse().button == ftxui::Mouse::WheelDown)
        {
            _model.scroll_down();
            return true;
        }
    }

    return false;
}

const CommandPaletteModel& InputController::command_palette() const
{
    return _command_palette_controller.model();
}

bool InputController::copy_selection_to_clipboard() const
{
    const auto text = _model.selection_text();
    if (text.empty())
    {
        return false;
    }

#ifdef _WIN32
    const int wide_length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wide_length <= 0)
    {
        return false;
    }

    auto* memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wide_length) * sizeof(wchar_t));
    if (memory == nullptr)
    {
        return false;
    }

    auto* wide_text = static_cast<wchar_t*>(GlobalLock(memory));
    if (wide_text == nullptr)
    {
        GlobalFree(memory);
        return false;
    }

    const int converted = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide_text, wide_length);
    GlobalUnlock(memory);
    if (converted <= 0)
    {
        GlobalFree(memory);
        return false;
    }

    if (!OpenClipboard(nullptr))
    {
        GlobalFree(memory);
        return false;
    }

    EmptyClipboard();
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr)
    {
        CloseClipboard();
        GlobalFree(memory);
        return false;
    }

    CloseClipboard();
    return true;
#else
    return copy_text_to_clipboard_on_unix(text);
#endif
}

} // namespace slayerlog
