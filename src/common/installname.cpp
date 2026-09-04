/*
 *  installname.cpp -- see installname.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "installname.h"

extern "C" {
#include "archive.h"
}

#include <string.h>

bool install_has_zip_suffix(const char *name) {
    /* One definition of "an archive the launcher can open", in archive.c,
     * so the folder scan and the reader cannot disagree about what is
     * worth opening. */
    return archive_has_suffix(name) != 0;
}

std::string install_base_name(const std::string &path) {
    size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

std::string install_safe_folder_name(const std::string &raw) {
    std::string out;

    for (size_t i = 0; i < raw.size(); i++) {
        unsigned char c = (unsigned char)raw[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '(' || c == ')') {
            out += (char)c;
        } else if (c == ' ' || c == '+') {
            /* Same rule as below: never two separators in a row.  "[Group]
             * Title" folded the bracket and then the space, and came out
             * with a double underscore in it. */
            if (!out.empty() && out[out.size() - 1] != '_') out += '_';
        } else if (!out.empty() && out[out.size() - 1] != '_') {
            out += '_';
        }
    }

    /* Trailing separators and dots confuse the filesystem. */
    while (!out.empty() && (out[out.size() - 1] == '_' ||
                            out[out.size() - 1] == '.'))
        out.erase(out.size() - 1);

    if (out.empty()) out = "game";
    if (out.size() > 64) out.erase(64);
    return out;
}

std::string install_destination_name(const std::string &zip_path) {
    std::string name = install_base_name(zip_path);
    /* >= rather than >: a file called nothing but ".zip" has no name to
     * keep, and folding what is left gives the fallback rather than a
     * hidden folder called ".zip". */
    /* ".zip" is four characters and ".7z" is three; asking rather than
     * assuming is what keeps a 7z from installing as "MyGam". */
    const int suffix = archive_suffix_length(name.c_str());
    if (suffix > 0) name.erase(name.size() - (size_t)suffix);
    return install_safe_folder_name(name);
}
