// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_LAYER_H
#define KRA_LAYER_H

#include "kra_tile.h"

#include <vector>

// KraLayer is a structure in which general properties for a KRA layer are stored.
// The actual image data is found in the tiles vector.
class KraLayer
{
public:
    enum KraLayerType
    {
        OTHER,
        PAINT_LAYER,
        VECTOR_LAYER,
        GROUP_LAYER
    };

    const wchar_t *name;
    const char *filename;

    unsigned int channelCount;
    unsigned int x;
    unsigned int y;

    uint8_t opacity;

    uint32_t type;
    bool isVisible;

    std::vector<std::unique_ptr<KraTile>> tiles;

    bool corruptionFlag = false;
};

#endif // KRA_LAYER_H