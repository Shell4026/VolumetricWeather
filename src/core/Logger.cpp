#include "core/Logger.h"

#include <chrono>
#include <filesystem>
void Logger::Info(std::string_view message, std::string_view name, int line)
{
    Log(0, message, name, line);
}

void Logger::Warn(std::string_view message, std::string_view name, int line)
{
    Log(1, message, name, line);
}

void Logger::Error(std::string_view message, std::string_view name, int line)
{
    Log(2, message, name, line);
}

void Logger::Log(int level, std::string_view message, std::string_view name, int line)
{
    using namespace std::chrono;

    auto now = system_clock::now();
    auto in_time_t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm buf;
    localtime_s(&buf, &in_time_t);

    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count();

    auto levelToStringFn =
        [](int level)
        {
            switch (level)
            {
            case 0:
                return "Info";
            case 1:
                return "Warn";
            case 2:
                return "Error";
            }
            return "Unknown";
        };

    std::string source{ std::filesystem::path{ std::string{name} }.filename().string() };

    std::string logMessage = "[" + oss.str() + "][" + levelToStringFn(level) + "]";
    if (!name.empty())
        logMessage += std::format("[{}: {}]", source, line);
    logMessage += message.data();
    logMessage += '\n';

    std::cout << logMessage;
}
