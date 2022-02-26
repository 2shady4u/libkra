// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_TILE_H
#define KRA_TILE_H

#include <memory>
#include <vector>

namespace kra
{
    // KraTile is a structure in which the decompressed binary data for a single tile is stored.
    // Each KRA layer consists of a vector of tiles that hold the actual image data.
    class KraTile
    {
    public:
        // These can also be negative!!!
        // The left (X) position of the tile.
        int32_t left;
        // The top (Y) position of the tile.
        int32_t top;

        // Number of compressed bytes that represent the tile data.
        int compressed_length;
        // Number of decompressed bytes that represent the tile data.
        // Always equal to tile_height * tile_width * pixel_size
        int decompressed_length;

        // Decompressed image data of this tile.
        std::unique_ptr<uint8_t[]> data;

        // Flag that gets raised when something goes wrong when parsing the data.
        // CURRENTLY UNUSED!
        bool corruption_flag = false;

        void import_attributes(const std::vector<unsigned char> &p_layer_content, unsigned int &p_index);

        void print_layer_attributes() const;
    };
};

#endif // KRA_TILE_H