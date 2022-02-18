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
        PAINT_LAYER,
        GROUP_LAYER
    };

    const wchar_t *name;
    const char *filename;

    const char *uuid;

    unsigned int channel_count;
    unsigned int x;
    unsigned int y;

    uint8_t opacity;

    KraLayerType type;
    bool visible;

    std::vector<std::unique_ptr<KraTile>> tiles;

    bool corruption_flag = false;

    virtual KraLayerType get_type() const = 0;
    virtual ~KraLayer() = default;
};

class KraPaintLayer : public KraLayer
{
public:
    KraLayerType get_type() const override
    {
        return KraLayerType::PAINT_LAYER;
    };
};

class KraGroupLayer : public KraLayer
{
public:
    std::vector<std::unique_ptr<KraLayer>> children;

    KraLayerType get_type() const override
    {
        return KraLayerType::GROUP_LAYER;
    };
};

#endif // KRA_LAYER_H