#pragma once

//=
// Simple logging system
//=

#include <iostream>
#include <string>

class Log
{
public:

    enum class Level
    {
        Info = 0,
        Warning = 1,
        Error = 2
    };

#ifndef NDEBUG

    static void SetLevel(Level lvl)
    {
        currentLevel = lvl;
    }

    static void Info(const std::string& msg)
    {
        Message(Level::Info, "[INFO] ", msg);
    }

    static void Warning(const std::string& msg)
    {
        Message(Level::Warning, "[WARNING] ", msg);
    }

#else

    static void SetLevel(Level) {}
    static void Info(const std::string&) {}
    static void Warning(const std::string&) {}

#endif

    static void Error(const std::string& msg)
    {
        std::cout << "[ERROR] " << msg << std::endl;
    }

private:

#ifndef NDEBUG

    static void Message(Level lvl,
                        const char* prefix,
                        const std::string& msg)
    {
        if (lvl < currentLevel)
            return;

        std::cout << prefix << msg << std::endl;
    }

    static Level currentLevel;

#endif
};