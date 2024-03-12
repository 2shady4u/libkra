// ############################################################################ #
// Copyright © 2022-2024 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_UTILITY_H
#define KRA_UTILITY_H

#include "../zlib/contrib/minizip/unzip.h"

#include <cstring> // std::memcpy
#include <string>
#include <vector>

#define WRITEBUFFERSIZE (8192)

namespace kra
{
    enum LayerType
    {
        PAINT_LAYER,
        GROUP_LAYER
    };

    enum ColorSpace
    {
        RGBA,
        RGBA16,
        RGBAF16,
        RGBAF32,
        CMYK,
        OTHER
    };

    enum VerbosityLevel
    {
        QUIET,
        NORMAL,
        VERBOSE,
        VERY_VERBOSE
    };

    extern VerbosityLevel verbosity_level;

    int extract_current_file_to_vector(unzFile &p_file, std::vector<unsigned char> &p_result);

    ColorSpace get_color_space(const std::string &p_color_space_name);

    const std::string get_color_space_name(ColorSpace p_color_space);
};

#endif // KRA_UTILITY_H