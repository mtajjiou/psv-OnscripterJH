/*
 *  installname.cpp -- see installname.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "installname.h"

#include <string.h>

bool install_has_zip_suffix(const char *name) {
    size_t len = name ? strlen(name) : 0;
    if (len < 4) return false;

    const char *ext = name + len - 4;
    return ext[0] == '.' &&
           (ext[1] == 'z' || ext[1] == 'Z') &&
           (ext[2] == 'i' || ext[2] == 'I') &&
           (ext[3] == 'p' || ext[3] == 'P');
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
    if (name.size() >= 4 && install_has_zip_suffix(name.c_str()))
        name.erase(name.size() - 4);
    return install_safe_folder_name(name);
}
