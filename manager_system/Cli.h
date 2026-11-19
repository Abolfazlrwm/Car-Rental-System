/**
 * ============================================================
 *  Cli — command-line argument parsing (v3.3.0)
 * ============================================================
 *  Pure, OS-free parsing for the program entry point, split out
 *  of mainApp.cpp so the rules are unit-testable like everything
 *  else. Non-interactive options (--help / --version) exit before
 *  touching data/.
 *
 *  Conventions:
 *    - long forms : --help  --version  --no-color
 *    - short forms: -h      -v
 *    - NO_COLOR   : honored as an environment variable by main()
 *                   (the https://no-color.org convention — present
 *                   and non-empty means plain output), because
 *                   environment state is not argv and stays out of
 *                   this parser.
 *    - precedence : --version beats --help when both are given
 *    - unknown    : error carries the offending argument; the
 *                   caller prints a hint and exits 1
 * ============================================================
 */
#pragma once

#include <string>

namespace cli
{

struct Options
{
    bool showHelp = false;    /// -h / --help: print usage, exit 0
    bool showVersion = false; /// -v / --version: print version, exit 0
    bool noColor = false;     /// --no-color: plain output for pipes, logs, CI
    std::string error;        /// non-empty -> usage error, names the argument
};

inline Options parse(int argc, char *argv[])
{
    Options opts;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
            opts.showHelp = true;
        else if (arg == "-v" || arg == "--version")
            opts.showVersion = true;
        else if (arg == "--no-color")
            opts.noColor = true;
        else
        {
            opts.error = "unknown option '" + arg + "'";
            return opts;
        }
    }
    return opts;
}

} // namespace cli
