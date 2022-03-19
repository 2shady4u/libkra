// ############################################################################ #
// Copyright © 2022 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "kra_utility.h"

namespace kra
{
    VerbosityLevel verbosity_level = NORMAL;

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract the data content of the current file in the ZIP archive to a vector
    // ---------------------------------------------------------------------------------------------------------------------
    int extract_current_file_to_vector(unzFile &p_file, std::vector<unsigned char> &p_result)
    {
        size_t error_code = UNZ_ERRNO;
        unz_file_info64 file_info = {0};
        char filename_inzip[256] = {0};

        /* Get the required size necessary to store the file content. */
        error_code = unzGetCurrentFileInfo64(p_file, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
        size_t uncompressed_size = file_info.uncompressed_size;

        error_code = unzOpenCurrentFile(p_file);

        std::vector<unsigned char> buffer;
        buffer.resize(WRITEBUFFERSIZE);
        p_result.reserve((size_t)file_info.uncompressed_size);

        /* error_code also serves as the number of bytes that were read... */
        do
        {
            /* Read the data in parts of size WRITEBUFFERSIZE */
            /* and keep reading until the function returns zero or lower. */
            error_code = unzReadCurrentFile(p_file, buffer.data(), (unsigned int)buffer.size());
            if (error_code < 0 || error_code == 0)
                break;

            p_result.insert(p_result.end(), buffer.data(), buffer.data() + error_code);

        } while (error_code > 0);

        /* Be sure to close the file to avoid leakage. */
        error_code = unzCloseCurrentFile(p_file);

        return (int)error_code;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Try to match the input string with one of the constants of the ColorSpace-enum
    // ---------------------------------------------------------------------------------------------------------------------
    ColorSpace get_color_space(const std::string &p_color_space_name)
    {
        if (p_color_space_name.compare("RGBA") == 0)
        {
            return RGBA;
        }
        else if (p_color_space_name.compare("RGBA16") == 0)
        {
            return RGBA16;
        }
        else if (p_color_space_name.compare("RGBAF16") == 0)
        {
            return RGBAF16;
        }
        else if (p_color_space_name.compare("RGBAF32") == 0)
        {
            return RGBAF32;
        }
        else if (p_color_space_name.compare("CMYK") == 0)
        {
            return CMYK;
        }
        else
        {
            return OTHER;
        }
    }
    
    // ---------------------------------------------------------------------------------------------------------------------
    // Get a human-readable string version of the given ColorSpace-enum
    // ---------------------------------------------------------------------------------------------------------------------
    const std::string get_color_space_name(ColorSpace p_color_space)
    {
        switch(p_color_space)
        {
            case RGBA:
                return "RGBA";
            case RGBA16:
                return "RGBA16";
            case RGBAF16:
                return "RGBAF16";
            case RGBAF32:
                return "RGBAF32";
            case CMYK:
                return "CMYK";
            default:
                return "not supported";
        }
    }
};