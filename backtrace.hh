# pragma once

# include <string>
# include <stdexcept>

/// Error throwed by backtrace at the end (to end) or at any internal error of backtrace
class Error : std::logic_error
{
public:
    Error(const std::string& s)
        : std::logic_error(s)
    {}
};

/// Print the backtrace (requires gdb) and throw an Error exception
void backtrace(unsigned skip = 0);