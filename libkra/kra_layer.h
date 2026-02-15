// ############################################################################ #
// Copyright © 2022-2026 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_LAYER_H
#define KRA_LAYER_H

#include "kra_utility.h"

#include "kra_keyframe.h"
#include "kra_layer_data.h"
#include "kra_exported_layer.h"

#include "../tinyxml2/tinyxml2.h"
#include "../zlib/contrib/minizip/unzip.h"

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

        std::vector<std::unique_ptr<Keyframe>> _parse_keyframes(const std::string &p_name, unzFile &p_file);

    public:
        std::string filename;
        std::string name;
        std::string uuid;
        std::string keyframes;

        unsigned int x;
        unsigned int y;

        uint8_t opacity;

        bool visible = true;

        LayerType type;

        // PAINT_LAYER
        ColorSpace color_space = RGBA;
        std::unique_ptr<LayerData> layer_data;
        std::vector<std::unique_ptr<Keyframe>> keyframes_data;

        // GROUP_LAYER
        std::vector<std::unique_ptr<Layer>> children;

        void import_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);

        std::unique_ptr<ExportedLayer> get_exported_layer() const;
        std::vector<std::unique_ptr<ExportedLayer>> get_exported_frames() const;

        void print_layer_attributes() const;
    };
};

#endif // KRA_LAYER_H