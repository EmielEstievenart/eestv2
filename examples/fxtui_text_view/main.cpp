#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

#include "text_view_controller.hpp"
#include "text_view_model.hpp"
#include "text_view_view.hpp"

using namespace ftxui;

int main()
{
    auto screen = ScreenInteractive::Fullscreen();

    TextViewModel model;
    TextViewController controller(model);

    std::vector<std::string> initial_lines;
    initial_lines.reserve(200);
    for (int i = 1; i <= 200; ++i)
    {
        initial_lines.push_back("Line " + std::to_string(i) + " - this is sample content for scrolling.");
    }
    initial_lines.push_back("Line - this is sample content for scrolling. BUT ITS REEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALLLLLLLLLLLYYYY LOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOONNNNGGGG");

    controller.append_lines(initial_lines, 20);

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

    auto component = CatchEvent(renderer, [&](Event event) { return controller.parse_event(event, screen.ExitLoopClosure()); });

    screen.Loop(component);
    return 0;
}
