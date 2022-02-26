// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_LAYER_H
#define KRA_LAYER_H

#include "kra_utility.h"

#include "kra_tile.h"
#include "kra_exported_layer.h"

#include "../tinyxml2/tinyxml2.h"
#include "../zlib/contrib/minizip/unzip.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>

namespace kra
{
    // KraLayer is a structure in which general properties for a KRA layer are stored.
    // The actual image data is found in the tiles vector.
    class KraLayer
    {
    private:
        void _parse_tiles(std::vector<unsigned char> p_content);

        unsigned int _parse_header_element(std::vector<unsigned char> layerContent, const std::string &elementName, unsigned int &currentIndex);
        std::string _get_header_element(std::vector<unsigned char> layerContent, unsigned int &currentIndex);
        int _lzff_decompress(const void *input, int length, void *output, int maxout);

        void _import_paint_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);
        void _import_group_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);

        void _print_group_layer_attributes() const;

    public:
        kra::LayerType type;

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

        std::vector<std::unique_ptr<KraLayer>> children;

        void import_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);

        std::unique_ptr<KraExportedLayer> get_exported_layer();

        void print_layer_attributes() const;
    };
};

#endif // KRA_LAYER_H