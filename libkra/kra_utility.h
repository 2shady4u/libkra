#ifndef KRA_UTILITY_H
#define KRA_UTILITY_H

#include "../zlib/contrib/minizip/unzip.h"

#include <memory>
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
        CMYK
    };

    enum VerbosityLevel
    {
        QUIET,
        NORMAL,
        VERBOSE,
        VERY_VERBOSE
    };

    static VerbosityLevel verbosity_level = NORMAL;

    int extract_current_file_to_vector(unzFile &p_file, std::vector<unsigned char> &p_result);
};

#endif // KRA_UTILITY_H