#pragma once

#define SH_INFO(message) Logger::Info(message, __FILE__, __LINE__)
#define SH_INFO_FORMAT(message, ...) Logger::Info(std::format(message, __VA_ARGS__), __FILE__, __LINE__)
#define SH_WARN(message) Logger::Warn(message, __FILE__, __LINE__)
#define SH_WARN_FORMAT(message, ...) Logger::Warn(std::format(message, __VA_ARGS__), __FILE__, __LINE__)
#define SH_ERROR(message) Logger::Error(message, __FILE__, __LINE__)
#define SH_ERROR_FORMAT(message, ...) Logger::Error(std::format(message, __VA_ARGS__), __FILE__, __LINE__)

#include <string_view>
#include <iostream>
#include <format>
class Logger
{
public:
	static void Info(std::string_view message, std::string_view name, int line = 0);
	static void Warn(std::string_view message, std::string_view name, int line = 0);
	static void Error(std::string_view message, std::string_view name, int line = 0);
private:
	static void Log(int level, std::string_view message, std::string_view name, int line = 0);
};