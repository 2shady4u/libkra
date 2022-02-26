#include "kra_utility.h"

namespace kra
{
    // ---------------------------------------------------------------------------------------------------------------------
    // Extract the data content of the current file in the ZIP archive to a vector.
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
};