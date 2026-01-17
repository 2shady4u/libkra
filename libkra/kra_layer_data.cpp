// ############################################################################ #
// Copyright © 2022-2026 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "kra_layer_data.h"

namespace kra
{
    // ---------------------------------------------------------------------------------------------------------------------
    // Extract the layer's attributes and data from the file's raw binary content.
    // ---------------------------------------------------------------------------------------------------------------------
    void LayerData::import_attributes(const std::vector<unsigned char> &p_layer_content)
    {
        /* This code works with a global pointer index that gets incremented depending on element size */
        /* current_index obviously starts at zero and will be passed by reference */
        unsigned int current_index = 0;

        /* Extract the main header from the tiles */
        version = _get_element_value(p_layer_content, "VERSION ", current_index);
        tile_width = _get_element_value(p_layer_content, "TILEWIDTH ", current_index);
        tile_height = _get_element_value(p_layer_content, "TILEHEIGHT ", current_index);
        pixel_size = _get_element_value(p_layer_content, "PIXELSIZE ", current_index);
        unsigned int decompressed_length = pixel_size * tile_width * tile_height;

        if (verbosity_level >= VERBOSE)
        {
            // TODO: This needs to be re-enabled at some point
            // print_layer_data_attributes();
        }

        unsigned int number_of_tiles = _get_element_value(p_layer_content, "DATA ", current_index);

        for (unsigned int i = 0; i < number_of_tiles; i++)
        {
            std::unique_ptr<Tile> tile = std::make_unique<Tile>();

            /* Now it is time to extract & decompress the data */
            /* First the non-general element of the header needs to be extracted */
            std::string headerString = _get_header_line(p_layer_content, current_index);
            std::regex e("(-?\\d*),(-?\\d*),(\\w*),(\\d*)");
            std::smatch sm;
            std::regex_match(headerString, sm, e);

            /* This header contains: */
            /* 1. A number that defines the left position of the tile (CAN BE NEGATIVE!!!) */
            /* 2. A number that defines the top position of the tile (CAN BE NEGATIVE!!!) */
            /* 3. The string "LZF" which states the compression algorithm */
            /* 4. A number that defines the number of bytes of the data that comes after this header. */

            /* sm[0] is the full string match which we don't need */
            /* The left position of the tile */
            std::ssub_match base_sub_match = sm[1];
            tile->left = std::stoi(base_sub_match.str());
            /* The top position of the tile */
            base_sub_match = sm[2];
            tile->top = std::stoi(base_sub_match.str());
            /* NOTE: We don't really care about sm[3] since it is always 'LZF' */
            /* The number of compressed bytes coming after this header */
            base_sub_match = sm[4];
            tile->compressed_length = std::stoi(base_sub_match.str());

            /* Put all the data in a vector */
            std::vector<uint8_t> compressed_data(p_layer_content.begin() + current_index, p_layer_content.begin() + current_index + tile->compressed_length);
            tile->compressed_data = compressed_data;

            /* Add the compressed_length to the current_index so the next tile starts at the correct position */
            // Needs to be done BEFORE moving the pointer's ownership to the vector!
            current_index += tile->compressed_length;

            tiles.push_back(std::move(tile));
        }

        _update_dimensions();
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Decompress & compose the binary data of the entire layer
    // ---------------------------------------------------------------------------------------------------------------------
    std::vector<uint8_t> LayerData::get_composed_data(ColorSpace color_space) const
    {
        /* Allocate space for the output data! */
        const unsigned int decompressed_length = pixel_size * tile_width * tile_height;
        const unsigned int number_of_columns = get_width() / tile_width;
        const unsigned int number_of_rows = get_height() / tile_height;
        const unsigned int row_length = number_of_columns * pixel_size * tile_width;

        const size_t composed_length = number_of_columns * number_of_rows * decompressed_length;
        std::vector<uint8_t> composed_data(composed_length);

        /* Go through all the tiles and decompress their data */
        for (auto const &tile : tiles)
        {
            std::vector<uint8_t> unsorted_data(decompressed_length);
            /* Now... the first byte of the data is actually some sort of indicator of compression */
            /* As follows: */
            /* 0 -> No compression, the data is actually raw! */
            /* 1 -> The data was compressed using LZF */
            // NOTE: At the time of writing this byte cannot be changed from its default value (1) and thus the data will ALWAYS be compressed.
            _lzff_decompress(tile->compressed_data.data() + 1, tile->compressed_length, unsorted_data.data(), decompressed_length);

            // TODO: Conversion between color profiles could potentially be done here?

            /* Due to historical reasons the red and blue pixel values are swapped in the case of RGBA & RGBA16 */
            /* This is to be rectified by using a special vector with swapped values */
            std::vector<unsigned int> pixel_vector(pixel_size);
            std::iota(std::begin(pixel_vector), std::end(pixel_vector), 0);
            if (color_space == ColorSpace::RGBA)
            {
                unsigned int bytes_per_channel = 1;
                std::swap_ranges(pixel_vector.begin(), pixel_vector.begin() + bytes_per_channel, pixel_vector.begin() + 2 * bytes_per_channel);
            }
            else if (color_space == ColorSpace::RGBA16)
            {
                unsigned int bytes_per_channel = 2;
                std::swap_ranges(pixel_vector.begin(), pixel_vector.begin() + bytes_per_channel, pixel_vector.begin() + 2 * bytes_per_channel);
            }

            /* Data is saved in following format: */
            /* (R0 R1 R2...) (G0 G1 G2...) (B0 B1 B2...) (A0 A1 A2...)*/
            /* which is different from the wanted format: */
            /* (R0 G0 B0 A0) (R1 G1 B1 A1) (R2 G2 B2 A2)...*/

            /* We'll have to do some sorting as a result!*/
            std::vector<uint8_t> sorted_data(decompressed_length);
            int tile_area = tile_height * tile_width;
            for (int i = 0; i < tile_area; i++)
            {
                unsigned int real_index = 0;
                for (unsigned int j : pixel_vector)
                {
                    sorted_data[i * pixel_size + real_index] = unsorted_data[j * tile_area + i];
                    real_index++;
                }
            }

            /* Now we have to construct the data in such a way that all tiles are in the correct positions */
            const int relative_tile_top = tile->top - top;
            const int relative_tile_left = tile->left - left;
            const size_t size = pixel_size * tile_width;
            for (int row_index = 0; row_index < (int)tile_height; row_index++)
            {
                uint8_t *destination = composed_data.data() + row_index * row_length;
                destination += relative_tile_left * pixel_size;
                destination += relative_tile_top * row_length;
                uint8_t *source = sorted_data.data() + size * row_index;
                /* Copy the row of the tile to the composed data */
                std::memcpy(destination, source, size);
            }
        }

        return composed_data;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Helper functions for accessing this layer's dimensions
    // ---------------------------------------------------------------------------------------------------------------------
    unsigned int LayerData::get_width() const
    {
        return (unsigned int)(right - left);
    }

    unsigned int LayerData::get_height() const
    {
        return (unsigned int)(bottom - top);
    }

    int32_t LayerData::get_top() const
    {
        return top;
    }

    int32_t LayerData::get_left() const
    {
        return left;
    }

    int32_t LayerData::get_bottom() const
    {
        return bottom;
    }

    int32_t LayerData::get_right() const
    {
        return right;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Print layer attributes to the output console
    // ---------------------------------------------------------------------------------------------------------------------
    void LayerData::print_layer_data_attributes() const
    {
        // TODO: This needs to be updated & expanded!
        fprintf(stdout, "----- Tile properties (Main Header) are extracted and have following values:\n");
        fprintf(stdout, "   >> version = %i\n", version);
        fprintf(stdout, "   >> tile_width = %i\n", tile_width);
        fprintf(stdout, "   >> tile_height = %i\n", tile_height);
        fprintf(stdout, "   >> pixel_size = %i\n", pixel_size);
        // fprintf(stdout, "   >> decompressed_length = %i\n", decompressed_length);
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract a header line and match it with the element name
    // ---------------------------------------------------------------------------------------------------------------------
    unsigned int LayerData::_get_element_value(const std::vector<unsigned char> &p_layer_content, const std::string &p_element_name, unsigned int &p_index) const
    {
        unsigned int element_int_value = -1;
        /* First extract the header element */
        std::string element_value = _get_header_line(p_layer_content, p_index);
        /* Try to match the elementValue string */
        if (element_value.find(p_element_name) != std::string::npos)
        {
            size_t pos = element_value.find(p_element_name);
            /* If found then erase it from string */
            element_value.erase(pos, p_element_name.length());
            /* Dump it into the output variable using stoi */
            element_int_value = std::stoi(element_value);
        }
        else
        {
            fprintf(stdout, "WARNING: Missing header element in tile with name '%s'\n", p_element_name.c_str());
        }
        return element_int_value;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract a header line starting from the current_index until the next "0x0A"
    // ---------------------------------------------------------------------------------------------------------------------
    std::string LayerData::_get_header_line(const std::vector<unsigned char> &p_layer_content, unsigned int &p_index) const
    {
        unsigned int begin_index = p_index;
        /* Just go through the vector until you encounter "0x0A" (= the hex value of Line Feed) */
        while (p_layer_content.at(p_index) != (char)0x0A)
        {
            p_index++;
        }
        unsigned int end_index = p_index;
        /* Extract this header element */
        std::string element_value(p_layer_content.begin() + begin_index, p_layer_content.begin() + end_index);
        /* Increment the current_index pointer so that we skip the "0x0A" character */
        p_index++;

        return element_value;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Go through all the tiles and find the layer's extents
    // ---------------------------------------------------------------------------------------------------------------------
    void LayerData::_update_dimensions()
    {
        /* Reset all the dimensions back to 0 */
        top = 0;
        left = 0;
        bottom = 0;
        right = 0;

        /* Find the extents of the layer canvas */
        for (auto const &tile : tiles)
        {
            if (tile->left < left)
            {
                left = tile->left;
            }
            if (tile->left + (int32_t)tile_width > right)
            {
                right = tile->left + (int32_t)tile_width;
            }

            if (tile->top < top)
            {
                top = tile->top;
            }
            if (tile->top + (int32_t)tile_height > bottom)
            {
                bottom = tile->top + (int32_t)tile_height;
            }
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Decompression function for LZF copied directly (with minor modifications) from the Krita codebase (libs\image\tiles3\swap\kis_lzf_compression.cpp)
    // ---------------------------------------------------------------------------------------------------------------------
    int LayerData::_lzff_decompress(const void *input, const int length, void *output, int maxout) const
    {
        const unsigned char *ip = (const unsigned char *)input;
        const unsigned char *ip_limit = ip + length - 1;
        unsigned char *op = (unsigned char *)output;
        unsigned char *op_limit = op + maxout;
        unsigned char *ref;

        while (ip < ip_limit)
        {
            unsigned int ctrl = (*ip) + 1;
            unsigned int ofs = ((*ip) & 31) << 8;
            unsigned int len = (*ip++) >> 5;

            if (ctrl < 33)
            {
                /* literal copy */
                if (op + ctrl > op_limit)
                    return 0;

                /* crazy unrolling */
                if (ctrl)
                {
                    *op++ = *ip++;
                    ctrl--;

                    if (ctrl)
                    {
                        *op++ = *ip++;
                        ctrl--;

                        if (ctrl)
                        {
                            *op++ = *ip++;
                            ctrl--;

                            for (; ctrl; ctrl--)
                                *op++ = *ip++;
                        }
                    }
                }
            }
            else
            {
                /* back reference */
                len--;
                ref = op - ofs;
                ref--;

                if (len == 7 - 1)
                    len += *ip++;

                ref -= *ip++;

                if (op + len + 3 > op_limit)
                    return 0;

                if (ref < (unsigned char *)output)
                    return 0;

                *op++ = *ref++;
                *op++ = *ref++;
                *op++ = *ref++;
                if (len)
                    for (; len; --len)
                        *op++ = *ref++;
            }
        }

        return (int)(op - (unsigned char *)output);
    }
};