#ifndef KRA_UTILITY_H
#define KRA_UTILITY_H

#include "../zlib/contrib/minizip/unzip.h"

#include <memory>
#include <vector>
#include <string>

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
};

#endif // KRA_UTILITY_H