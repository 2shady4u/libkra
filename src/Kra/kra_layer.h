// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_LAYER_H
#define KRA_LAYER_H

#include "kra_tile.h"

#include "../tinyxml2/tinyxml2.h"

#include <vector>
#include <string>

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

    std::string filename;
    std::string name;
    std::string uuid;

    unsigned int channel_count;
    unsigned int x;
    unsigned int y;

    uint8_t opacity;

    bool corruption_flag = false;
    bool visible = true;

    std::vector<std::unique_ptr<KraTile>> tiles;

    void import_attributes(const tinyxml2::XMLElement *p_xml_element);

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