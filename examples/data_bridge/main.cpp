#include "eestv/data_bridge/bridge.hpp"
#include "eestv/data_bridge/command_param_parser.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include <boost/asio/io_context.hpp>

int main(int argc, char* argv[])
{
    auto config = eestv::bridge::parse_command_line(argc, argv);

    // Set the log level from the config
    eestv::logging::current_log_level = config.log_level;

    boost::asio::io_context io_context;
    eestv::bridge::Bridge data_bridge(config, io_context);

    // Run the io_context (user responsibility)
    io_context.run();

    return 0;
}