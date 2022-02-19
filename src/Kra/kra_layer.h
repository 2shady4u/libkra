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
#include "../../zlib/contrib/minizip/unzip.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>

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

    virtual void parse_tiles(std::vector<unsigned char> p_content) = 0;
    virtual void import_attributes(unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);

    virtual void print_layer_attributes() const;
    virtual KraLayerType get_type() const = 0;
    virtual ~KraLayer() = default;
};

class KraPaintLayer : public KraLayer
{
private:
    unsigned int _parse_header_element(std::vector<unsigned char> layerContent, const std::string &elementName, unsigned int &currentIndex);
    std::string _get_header_element(std::vector<unsigned char> layerContent, unsigned int &currentIndex);
    int _lzff_decompress(const void *input, int length, void *output, int maxout);

public:
    void parse_tiles(std::vector<unsigned char> p_content);
    void import_attributes(unzFile &p_file, const tinyxml2::XMLElement *p_xml_element) override;

    void print_layer_attributes() const override;
    KraLayerType get_type() const override
    {
        return KraLayerType::PAINT_LAYER;
    };
};

class KraGroupLayer : public KraLayer
{
public:
    std::vector<std::unique_ptr<KraLayer>> children;

    void parse_tiles(std::vector<unsigned char> p_content);
    void import_attributes(unzFile &p_file, const tinyxml2::XMLElement *p_xml_element) override;

    void print_layer_attributes() const override;
    KraLayerType get_type() const override
    {
        return KraLayerType::GROUP_LAYER;
    };
};

#endif // KRA_LAYER_H