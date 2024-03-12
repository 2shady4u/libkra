// ############################################################################ #
// Copyright © 2022-2024 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_LAYER_DATA_H
#define KRA_LAYER_DATA_H

#include "kra_utility.h"

#include <memory>
#include <regex>
#include <numeric>

#define WRITEBUFFERSIZE (8192)

namespace kra
{
    /* This class contains the actual data as stored in the layer's unique binary file */
    class LayerData
    {
    private:
        class Tile
        {
        public:
            // These can also be negative!!!
            // The left (X) position of the tile.
            int32_t left;
            // The top (Y) position of the tile.
            int32_t top;

            // Number of compressed bytes that represent the tile data.
            int compressed_length;

            // Compressed image data of this tile.
            std::vector<uint8_t> compressed_data;
        };

        std::vector<std::unique_ptr<Tile>> tiles;

        int32_t top;
        int32_t left;
        int32_t bottom;
        int32_t right;

        unsigned int _get_element_value(const std::vector<unsigned char> &p_layer_content, const std::string &p_element_name, unsigned int &p_index) const;
        std::string _get_header_line(const std::vector<unsigned char> &p_layer_content, unsigned int &p_index) const;

        void _update_dimensions();

        int _lzff_decompress(const void *input, const int length, void *output, int maxout) const;

    public:
        // Version statement of the layer, always equal to 2.
        unsigned int version;
        // Number of vertical pixels stored in each tile, always equal to 64.
        unsigned int tile_height;
        // Number of horizontal pixels stored in each tile, always equal to 64.
        unsigned int tile_width;
        // Number of elements in each pixel, is equal to 4 for RGBA.
        unsigned int pixel_size;

        void import_attributes(const std::vector<unsigned char> &p_layer_content);

        std::vector<uint8_t> get_composed_data(ColorSpace color_space) const;

        unsigned int get_width() const;
        unsigned int get_height() const;

        int32_t get_top() const;
        int32_t get_left() const;
        int32_t get_bottom() const;
        int32_t get_right() const;

        void print_layer_data_attributes() const;
    };
};

#endif // KRA_LAYER_DATA_H