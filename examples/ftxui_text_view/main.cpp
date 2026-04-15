#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_view.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace ftxui;

int main()
{
    auto screen = ScreenInteractive::Fullscreen();

    // The app owns the line data and exposes it through the controller callback.
    std::vector<std::string> lines;
    lines.reserve(201);
    for (int i = 1; i <= 200; ++i)
    {
        lines.push_back("Line " + std::to_string(i) + " - this is sample content for scrolling.");
    }
    lines.push_back(
        "Line - this is sample content for scrolling. BUT ITS "
        "REEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAALLLLLLLLLLLYYYY LOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOONNNNGGGG");

    int max_line_width = 0;
    for (const auto& line : lines)
    {
        max_line_width = std::max(max_line_width, static_cast<int>(line.size()));
    }

    TextViewController controller;
    controller.set_content(static_cast<int>(lines.size()), max_line_width, [&](int index) -> const std::string& { return lines[static_cast<std::size_t>(index)]; });

    // Highlight columns 5-9 ("1 - t" on line 1, the digit+separator region) in red.
    // Press 'h' at runtime to toggle the highlight on and off.
    bool highlight_on = true;
    controller.set_background_column_range(5, 9, Color::Red);

    TextViewView view;

    auto renderer = Renderer(
        [&]
        {
            controller.update_viewport_line_count(std::max(1, view.viewport_line_count()));
            controller.update_viewport_col_count(std::max(1, view.viewport_col_count()));
            const auto data = controller.render_data();

            const auto draw_content = [&](Canvas& canvas, int first_line, int line_count, int first_col, int col_count)
            {
                for (int row = 0; row < line_count; ++row)
                {
                    const std::string& line = lines[static_cast<std::size_t>(first_line + row)];
                    if (first_col >= static_cast<int>(line.size()))
                    {
                        continue;
                    }

                    canvas.DrawText(0, row * 4, line.substr(static_cast<std::size_t>(first_col), static_cast<std::size_t>(col_count)));
                }
            };

            return vbox({
                       text("Hello"), view.render(data, draw_content) | yflex | border,
                       text("World") // <-- after
                   }) |
                   flex;
        });

    auto component = CatchEvent(renderer,
                                [&](Event event)
                                {
                                    if (event == Event::Character('h'))
                                    {
                                        highlight_on = !highlight_on;
                                        if (highlight_on)
                                            controller.set_background_column_range(5, 9, Color::Red);
                                        else
                                            controller.clear_background_column_range();
                                        return true;
                                    }
                                    auto result = controller.parse_event(event);
                                    if (result.request_exit)
                                    {
                                        screen.Exit();
                                    }
                                    return result.handled;
                                });

    screen.Loop(component);
    return 0;
}
