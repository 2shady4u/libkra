// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_EXPORTED_LAYER_H
#define KRA_EXPORTED_LAYER_H

#include <string>

// KraExportedLayer is a structure in which the decompressed binary data for the entire image is saved.
// Needless to say... these structures can become quite big...
class KraExportedLayer
{
public:
    std::string name;

    unsigned int channel_count;
    unsigned int x;
    unsigned int y;

    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;

    uint8_t opacity;

    bool visible;

    std::unique_ptr<uint8_t[]> data;
};

#endif // KRA_EXPORTED_LAYER_H