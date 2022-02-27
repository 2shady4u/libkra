// ############################################################################ #
// Copyright © 2022 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_LAYER_H
#define KRA_LAYER_H

#include "kra_utility.h"

#include "kra_layer_data.h"
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
    /* This class stores the attributes (as found in 'maindoc.xml') for a single layer */
    /* The exact same class is used for both PAINT_LAYER and GROUP_LAYER to reduce code complexity */
    class Layer
    {
    private:
        void _import_paint_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);
        void _import_group_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);

        void _print_paint_layer_attributes() const;
        void _print_group_layer_attributes() const;

    public:
        std::string filename;
        std::string name;
        std::string uuid;

        unsigned int x;
        unsigned int y;

        uint8_t opacity;

        bool visible = true;

        LayerType type;

        // PAINT_LAYER
        ColorSpace color_space = RGBA;
        std::unique_ptr<LayerData> layer_data;

        // GROUP_LAYER
        std::vector<std::unique_ptr<Layer>> children;

        void import_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);

        std::unique_ptr<ExportedLayer> get_exported_layer() const;

        void print_layer_attributes() const;
    };
};

#endif // KRA_LAYER_H