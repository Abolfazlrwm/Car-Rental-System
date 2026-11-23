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
 *    - long forms : --help  --version  --no-color  --lang fa|en
 *    - short forms: -h      -v
 *    - NO_COLOR   : honored as an environment variable by main()
 *                   (the https://no-color.org convention — present
 *                   and non-empty means plain output), because
 *                   environment state is not argv and stays out of
 *                   this parser.
 *    - precedence : --version beats --help when both are given
 *    - --demo     : (v3.4.0) plays the narrated guided walkthrough of
 *                   one full shift on a fresh in-memory fleet
 *    - --lang     : (v3.6.0) UI language for the interactive layer,
 *                   "fa" or "en", either "--lang fa" or "--lang=fa";
 *                   the value is validated (and persisted) by main()
 *    - --serve    : (v3.7.0) run the read-only REST API instead of
 *                   the menu loop; optional port, default 8080
 *    - unknown    : error carries the offending argument; the
 *                   caller prints a hint and exits 1
 * ============================================================
 */
#pragma once

#include <cstdlib>
#include <string>

namespace cli
{

struct Options
{
    bool showHelp = false;    /// -h / --help: print usage, exit 0
    bool showVersion = false; /// -v / --version: print version, exit 0
    bool noColor = false;     /// --no-color: plain output for pipes, logs, CI
    bool demo = false;        /// --demo (v3.4.0): narrated guided walkthrough
    std::string lang;         /// --lang (v3.6.0): "fa" | "en"; empty = auto
    int servePort = 0;        /// --serve (v3.7.0): REST port; 0 = off
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
        else if (arg == "--demo")
            opts.demo = true;
        else if (arg == "--lang" || arg.rfind("--lang=", 0) == 0)
        {
            if (arg.size() > 7) // --lang=fa form
                opts.lang = arg.substr(7);
            else if (i + 1 < argc)
                opts.lang = argv[++i];
            else
            {
                opts.error = "--lang needs a value (fa|en)";
                return opts;
            }
        }
        else if (arg == "--serve" || arg.rfind("--serve=", 0) == 0)
        {
            // "--serve" alone means the default 8080; a following
            // all-digit argument is the port, "--serve=PORT" too.
            std::string value;
            bool hasValue = false;
            if (arg.size() > 8)
            {
                value = arg.substr(8);
                hasValue = true;
            }
            else if (i + 1 < argc)
            {
                const std::string next = argv[i + 1];
                if (!next.empty() &&
                    next.find_first_not_of("0123456789") == std::string::npos)
                {
                    value = next;
                    ++i;
                    hasValue = true;
                }
            }
            opts.servePort = hasValue ? std::atoi(value.c_str()) : 8080;
            if (opts.servePort < 1 || opts.servePort > 65535)
            {
                opts.error = "--serve needs a port between 1 and 65535";
                return opts;
            }
        }
        else
        {
            opts.error = "unknown option '" + arg + "'";
            return opts;
        }
    }
    return opts;
}

} // namespace cli
