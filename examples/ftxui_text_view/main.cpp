#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_model.hpp>
#include <ftxui_components/text_view_view.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

using namespace ftxui;

int main()
{
    auto screen = ScreenInteractive::Fullscreen();

    // The app owns the line data. The model is a non-owning view.
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

    TextViewModel model;
    TextViewController controller(model);
    controller.swap_lines(lines);

    // Highlight columns 5-9 ("1 - t" on line 1, the digit+separator region) in red.
    // Press 'h' at runtime to toggle the highlight on and off.
    bool highlight_on = true;
    controller.set_background_column_range(5, 9, Color::Red);

    TextViewView view(controller);

    auto renderer = Renderer(
        [&]
        {
            return vbox({
                       text("Hello"), view.component() | yflex | border,
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
