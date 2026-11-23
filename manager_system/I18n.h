/**
 * ============================================================
 *  I18n — console string table, English / Persian (v3.6.0)
 * ============================================================
 *  The interaction layer (menus, prompts, tables, reports that
 *  mainApp renders) speaks two languages; the domain core does
 *  not. Keys are the ENGLISH literals themselves, so a missing
 *  translation degrades gracefully to the exact string the
 *  suite has always pinned:
 *
 *      std::cout << L("  Logged out.\n");   // FA: خروج از حساب
 *
 *  Language selection:
 *    - `--lang fa` / `--lang en` on the command line (wins),
 *      persisted to ./data/lang.txt for the next runs;
 *    - otherwise the persisted file (if any);
 *    - otherwise English — which keeps the CI contract greps
 *      (--help, --demo narration markers) byte-stable.
 *
 *  Out of scope by design (still English in every language):
 *    - --help / --version output and the guided-demo narration
 *      (they ARE the machine contract the CI job greps);
 *    - core-templated strings (booking errors, discountNote,
 *      reminderPhrase, toString(plan)) — unit tests pin their
 *      exact bytes; mapping them happens through the UI when
 *      it matters.
 * ============================================================
 */
#pragma once

#include <string>

namespace i18n
{

enum class Lang
{
    En,
    Fa
};

/// Set the active language (no persistence).
void setLang(Lang lang);

/// "fa" / "en" (case-insensitive). Unknown names keep the current
/// language and return false.
bool setLangByName(const std::string &name);

Lang lang();

/// Translate an English UI string. Unknown keys (and the whole
/// table in En mode) return the key unchanged — every call site
/// degrades to the original English output.
const char *tr(const char *key);

/// Same, for keys built at runtime (std::string). Returns "" only
/// for an empty input, never for an unknown key.
std::string trStr(const std::string &key);

/// Read ./data/lang.txt (silently ignores a missing/corrupt file).
void loadPersisted();

/// Write the current language to ./data/lang.txt (creates the
/// directory). False when the file cannot be written.
bool persist();

/// "fa" | "en" — for diagnostics and the seed hint.
const char *langCode();

} // namespace i18n

/// Translate-at-callsite shorthand used across mainApp.cpp.
#define L(s) i18n::tr(s)
