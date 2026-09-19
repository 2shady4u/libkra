// ############################################################################ #
// Copyright © 2022-2026 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
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

namespace kra
{
    /* This class stores the attributes (as found in 'maindoc.xml') for a single layer */
    /* The exact same class is used for both PAINT_LAYER and GROUP_LAYER to reduce code complexity */
    class Layer
    {
    private:
        void _import_paint_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);
        void _import_group_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);
        void _import_vector_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);

        void _print_paint_layer_attributes() const;
        void _print_group_layer_attributes() const;
        void _print_vector_layer_attributes() const;

        void _compose_paint_layer(unsigned int p_document_width, unsigned int p_document_height, uint8_t *p_rgba_inout) const;
        void _compose_vector_layer(unsigned int p_document_width, unsigned int p_document_height, double p_document_dpi, uint8_t *p_rgba_inout) const;

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

        // VECTOR_LAYER
        std::vector<unsigned char> svg_content;

        void import_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);

        std::unique_ptr<ExportedLayer> get_exported_layer() const;

        void print_layer_attributes() const;

        /* Blend this layer, and for a group layer its children, over an RGBA8 buffer holding the */
        /* whole document. p_rgba_inout must be p_document_width * p_document_height * 4 bytes. */
        /* Layers are blended over whatever is already in the buffer, so to composite a document */
        /* call this on each of Document::layers from the bottom one upwards. */
        /* p_document_dpi is Document::x_res, and scales vector layers, whose SVG content is */
        /* authored against SVG's own 96 DPI. */
        void compose(unsigned int p_document_width, unsigned int p_document_height, double p_document_dpi, uint8_t *p_rgba_inout) const;
    };
};

#endif // KRA_LAYER_H
