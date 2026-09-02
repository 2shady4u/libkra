// ############################################################################ #
// Copyright © 2022-2026 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "kra_layer.h"

#include <lunasvg.h>

namespace kra
{
    // ---------------------------------------------------------------------------------------------------------------------
    // Extract important common attributes as stored in this layer's XML element
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::import_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
    {
        /* Get important layer attributes from the XML-file */
        filename = p_xml_element->Attribute("filename");
        name = p_xml_element->Attribute("name");
        uuid = p_xml_element->Attribute("uuid");

        x = p_xml_element->UnsignedAttribute("x", 0);
        y = p_xml_element->UnsignedAttribute("y", 0);
        opacity = p_xml_element->UnsignedAttribute("opacity", 0);

        visible = p_xml_element->BoolAttribute("visible", true);

        switch (type)
        {
        case PAINT_LAYER:
            _import_paint_attributes(p_name, p_file, p_xml_element);
            break;
        case GROUP_LAYER:
            _import_group_attributes(p_name, p_file, p_xml_element);
            break;
        case VECTOR_LAYER:
            _import_vector_attributes(p_name, p_file, p_xml_element);
            break;
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Get an exported version of this layer that can be used by other (external) programs & wrappers
    // ---------------------------------------------------------------------------------------------------------------------
    std::unique_ptr<ExportedLayer> Layer::get_exported_layer() const
    {
        std::unique_ptr<ExportedLayer> exported_layer = std::make_unique<ExportedLayer>();

        /* Copy all important properties immediately */
        exported_layer->name = name;
        exported_layer->x = x;
        exported_layer->y = y;
        exported_layer->opacity = opacity;
        exported_layer->visible = visible;
        exported_layer->type = type;

        switch (type)
        {
        case PAINT_LAYER:
        {
            exported_layer->color_space = color_space;

            exported_layer->top = layer_data->get_top();
            exported_layer->left = layer_data->get_left();
            exported_layer->bottom = layer_data->get_bottom();
            exported_layer->right = layer_data->get_right();

            exported_layer->pixel_size = layer_data->pixel_size;

            exported_layer->data = layer_data->get_composed_data(color_space);

            exported_layer->default_pixel = layer_data->get_composed_default_pixel(color_space);
            break;
        }
        case GROUP_LAYER:
            for (auto const &child : children)
            {
                exported_layer->child_uuids.push_back(child->uuid);
            }
            break;
        case VECTOR_LAYER:
            /* Hand the raw SVG document to the application, which is free to rasterize it however it likes */
            exported_layer->svg_content = svg_content;
            break;
        }

        return exported_layer;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Print layer attributes to the output console
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::print_layer_attributes() const
    {
        fprintf(stdout, "----- Layer with name '%s' contains following values:\n", name.c_str());
        fprintf(stdout, "   >> filename = %s\n", filename.c_str());
        fprintf(stdout, "   >> name = %s\n", name.c_str());
        fprintf(stdout, "   >> uuid = %s\n", uuid.c_str());
        fprintf(stdout, "   >> x = %i\n", x);
        fprintf(stdout, "   >> y = %i\n", y);
        fprintf(stdout, "   >> opacity = %i\n", opacity);
        fprintf(stdout, "   >> visible = %s\n", visible ? "true" : "false");
        fprintf(stdout, "   >> type = %i\n", type);

        switch (type)
        {
        case PAINT_LAYER:
            _print_paint_layer_attributes();
            break;
        case GROUP_LAYER:
            _print_group_layer_attributes();
            break;
        case VECTOR_LAYER:
            _print_vector_layer_attributes();
            break;
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract attributes specific to this layer's type (= PAINT_LAYER) and create a LayerData-instance
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::_import_paint_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
    {
        std::string color_space_name = p_xml_element->Attribute("colorspacename");
        /* The color space defines the number of 'channels' */
        /* Each seperate layer can have its own color space in KRA, but this doesn't seem to used by default */
        color_space = get_color_space(color_space_name);

        /* Try and find the relevant file that defines this layer's tile data */
        /* This also automatically decrypts the tile data */
        /* The "Sample/"-folder is hard-coded as I have yet to encounter a case where this folder is named differently! */
        const std::string &layer_path = p_name + "/layers/" + filename;
        std::vector<unsigned char> layer_content;
        const char *c_path = layer_path.c_str();
        int errorCode = unzLocateFile(p_file, c_path, 1);
        errorCode += extract_current_file_to_vector(p_file, layer_content);
        if (errorCode == UNZ_OK)
        {
            /* Start extracting the tile data. */
            layer_data = std::make_unique<LayerData>();
            layer_data->import_attributes(layer_content);

            /* A layer only stores tiles for the region it actually painted. Every pixel outside of that region */
            /* has the layer's default pixel colour, which is stored in a separate entry next to the tile data. */
            /* The entry is optional: layers that are fully transparent outside their tiles simply omit it. */
            const std::string &default_pixel_path = layer_path + ".defaultpixel";
            std::vector<unsigned char> default_pixel_content;
            if (unzLocateFile(p_file, default_pixel_path.c_str(), 1) == UNZ_OK &&
                extract_current_file_to_vector(p_file, default_pixel_content) == UNZ_OK &&
                default_pixel_content.size() == layer_data->pixel_size)
            {
                layer_data->default_pixel.assign(default_pixel_content.begin(),
                                                 default_pixel_content.begin() + layer_data->pixel_size);
            }
            else
            {
                layer_data->default_pixel.assign(layer_data->pixel_size, 0);
            }
        }
        else
        {
            fprintf(stdout, "ERROR: Layer entry with path '%s' could not be found in KRA archive.\n", layer_path.c_str());
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract attributes specific to this layer's type (= GROUP_LAYER) and recursively import child layers
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::_import_group_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
    {
        const tinyxml2::XMLElement *layers_element = p_xml_element->FirstChildElement("layers");
        const tinyxml2::XMLNode *first_child = layers_element->FirstChild();
        const tinyxml2::XMLElement *layer_node = (first_child != 0) ? first_child->ToElement() : 0;

        while (layer_node != 0)
        {
            /* Check the type of the layer and proceed from there... */
            std::string node_type = layer_node->Attribute("nodetype");
            std::unique_ptr<Layer> layer = std::make_unique<Layer>();

            /* Try to determine the layer type from the node type string */
            if (layer_type_from_string(node_type, &layer->type))
            {
                layer->import_attributes(p_name, p_file, layer_node);

                if (verbosity_level >= VERBOSE)
                {
                    layer->print_layer_attributes();
                }

                children.push_back(std::move(layer));
            }

            /* Try to get the next layer entry... if not available break */
            const tinyxml2::XMLNode *nextSibling = layer_node->NextSibling();
            if (nextSibling == 0)
            {
                break;
            }
            else
            {
                layer_node = nextSibling->ToElement();
            }
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Print additional attributes specific to this layer's type (= PAINT_LAYER) to the output console
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::_print_paint_layer_attributes() const
    {
        fprintf(stdout, "   -- Additional attributes specific to this layer's type (= PAINT_LAYER):\n");
        fprintf(stdout, "   >> color_space = %i\n", color_space);
        if (layer_data)
        {
            fprintf(stdout, "   >> default_pixel =");
            for (uint8_t channel : layer_data->get_composed_default_pixel(color_space))
            {
                fprintf(stdout, " %i", channel);
            }
            fprintf(stdout, "\n");
        }
        // TODO: Also print attributes of the layer_data!
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Print additional attributes specific to this layer's type (= GROUP_LAYER) to the output console
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::_print_group_layer_attributes() const
    {
        fprintf(stdout, "   -- Additional attributes specific to this layer's type (= GROUP_LAYER):\n");
        fprintf(stdout, "   >> children:\n");
        for (const auto &layer : children)
        {
            fprintf(stdout, "      - '%s' (%s)\n", layer->name.c_str(), layer->uuid.c_str());
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract attributes specific to this layer's type (= VECTOR_LAYER) and load the SVG content
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::_import_vector_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
    {
        /* Vector layers are stored in a directory named <filename>.shapelayer/content.svg */
        const std::string &svg_path = p_name + "/layers/" + filename + ".shapelayer/content.svg";
        const char *c_path = svg_path.c_str();
        int errorCode = unzLocateFile(p_file, c_path, 1);
        errorCode += extract_current_file_to_vector(p_file, svg_content);
        if (errorCode != UNZ_OK)
        {
            fprintf(stdout, "ERROR: Vector layer SVG content with path '%s' could not be found in KRA archive.\n", svg_path.c_str());
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Print additional attributes specific to this layer's type (= VECTOR_LAYER) to the output console
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::_print_vector_layer_attributes() const
    {
        fprintf(stdout, "   -- Additional attributes specific to this layer's type (= VECTOR_LAYER):\n");
        fprintf(stdout, "   >> svg_content size = %zu bytes\n", svg_content.size());
        if (!svg_content.empty())
        {
            fprintf(stdout, "   >> SVG data loaded successfully\n");
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Read one pixel of a layer's color space as RGBA8
    // ---------------------------------------------------------------------------------------------------------------------
    static void _read_pixel_as_rgba8(const uint8_t *p_src, ColorSpace p_color_space, uint8_t *p_dst)
    {
        if (p_color_space == RGBA16)
        {
            /* Krita writes tile data straight out of the paint device without any byte swapping, */
            /* so a 16 bit channel is stored in host order, i.e. little endian in practice. */
            /* See KisTileCompressor2::compressTileData(). An 8 bit value b maps to b * 257, so */
            /* dividing by 257 with rounding is the exact inverse. */
            for (unsigned int i = 0; i < 4; i++)
            {
                const unsigned int value = (unsigned int)p_src[i * 2] | ((unsigned int)p_src[i * 2 + 1] << 8);
                p_dst[i] = (uint8_t)((value + 128) / 257);
            }
            return;
        }

        p_dst[0] = p_src[0];
        p_dst[1] = p_src[1];
        p_dst[2] = p_src[2];
        p_dst[3] = p_src[3];
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Blend a single source pixel, scaled by the layer opacity, over a destination pixel (src-over)
    // ---------------------------------------------------------------------------------------------------------------------
    static void _blend_over(const uint8_t *p_src, uint8_t p_opacity, uint8_t *p_dst)
    {
        const float src_alpha = (p_src[3] * p_opacity / 255) / 255.0f;
        if (src_alpha <= 0.0f)
        {
            /* A fully transparent source cannot change the destination. */
            return;
        }

        const float dst_alpha = p_dst[3] / 255.0f;
        const float out_alpha = src_alpha + dst_alpha * (1.0f - src_alpha);
        if (out_alpha <= 0.0f)
        {
            return;
        }

        for (unsigned int i = 0; i < 3; i++)
        {
            p_dst[i] = (uint8_t)((p_src[i] * src_alpha + p_dst[i] * dst_alpha * (1.0f - src_alpha)) / out_alpha + 0.5f);
        }
        p_dst[3] = (uint8_t)(out_alpha * 255.0f + 0.5f);
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Blend this layer over an RGBA8 buffer holding the whole document
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::compose(unsigned int p_document_width, unsigned int p_document_height, double p_document_dpi, uint8_t *p_rgba_inout) const
    {
        if (!visible || opacity == 0)
        {
            return;
        }

        switch (type)
        {
        case PAINT_LAYER:
            _compose_paint_layer(p_document_width, p_document_height, p_rgba_inout);
            break;
        case VECTOR_LAYER:
            _compose_vector_layer(p_document_width, p_document_height, p_document_dpi, p_rgba_inout);
            break;
        case GROUP_LAYER:
            /* Krita stores layers top-first, so walk the children in reverse to blend bottom-up. */
            for (auto child = children.rbegin(); child != children.rend(); ++child)
            {
                (*child)->compose(p_document_width, p_document_height, p_document_dpi, p_rgba_inout);
            }
            break;
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Blend the tile data, and the default pixel covering everything outside of it, over the document
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::_compose_paint_layer(unsigned int p_document_width, unsigned int p_document_height, uint8_t *p_rgba_inout) const
    {
        if (!layer_data)
        {
            return;
        }

        /* The buffer we compose into is RGBA8, so a wider layer is narrowed one pixel at a time */
        /* as it is blended, which avoids building a second full size copy of the layer. */
        /* The float and CMYK spaces need a different conversion and are not handled yet. */
        const unsigned int bytes_per_pixel = (color_space == RGBA16) ? 8 : 4;
        if ((color_space != RGBA && color_space != RGBA16) || layer_data->pixel_size != bytes_per_pixel)
        {
            fprintf(stderr, "ERROR: Layer '%s' cannot be composed, only the RGBA and RGBA16 color spaces are supported.\n", name.c_str());
            return;
        }

        const std::vector<uint8_t> layer_pixels = layer_data->get_composed_data(color_space);
        const std::vector<uint8_t> default_pixel = layer_data->get_composed_default_pixel(color_space);

        const int32_t layer_left = layer_data->get_left();
        const int32_t layer_top = layer_data->get_top();
        const unsigned int layer_width = layer_data->get_width();
        const unsigned int layer_height = layer_data->get_height();

        /* The tiles only cover the region the layer actually painted; every pixel outside of that */
        /* region takes the default pixel, which is why a solid background layer can store no tiles */
        /* at all. A fully transparent default pixel, which is also what an archive without a */
        /* .defaultpixel entry yields, cannot contribute and lets us skip the untiled region. */
        bool default_pixel_contributes = false;
        if (default_pixel.size() >= bytes_per_pixel)
        {
            uint8_t default_rgba8[4];
            _read_pixel_as_rgba8(default_pixel.data(), color_space, default_rgba8);
            default_pixel_contributes = default_rgba8[3] != 0;
        }

        if (default_pixel_contributes)
        {
            for (unsigned int dy = 0; dy < p_document_height; dy++)
            {
                for (unsigned int dx = 0; dx < p_document_width; dx++)
                {
                    /* Inside the tiles the stored pixel replaces the default pixel, it does not */
                    /* sit on top of it, so pick exactly one source per document pixel. */
                    const int64_t lx = (int64_t)dx - (int64_t)x - layer_left;
                    const int64_t ly = (int64_t)dy - (int64_t)y - layer_top;

                    const uint8_t *src = default_pixel.data();
                    if (!layer_pixels.empty() &&
                        lx >= 0 && lx < (int64_t)layer_width &&
                        ly >= 0 && ly < (int64_t)layer_height)
                    {
                        src = &layer_pixels[((size_t)ly * layer_width + (size_t)lx) * bytes_per_pixel];
                    }

                    uint8_t src_rgba8[4];
                    _read_pixel_as_rgba8(src, color_space, src_rgba8);
                    _blend_over(src_rgba8, opacity, &p_rgba_inout[((size_t)dy * p_document_width + dx) * 4]);
                }
            }
            return;
        }

        if (layer_pixels.empty())
        {
            return;
        }

        for (unsigned int ly = 0; ly < layer_height; ly++)
        {
            for (unsigned int lx = 0; lx < layer_width; lx++)
            {
                const int64_t doc_x = (int64_t)x + layer_left + lx;
                const int64_t doc_y = (int64_t)y + layer_top + ly;

                if (doc_x < 0 || doc_x >= (int64_t)p_document_width ||
                    doc_y < 0 || doc_y >= (int64_t)p_document_height)
                {
                    continue;
                }

                uint8_t src_rgba8[4];
                _read_pixel_as_rgba8(&layer_pixels[((size_t)ly * layer_width + lx) * bytes_per_pixel], color_space, src_rgba8);
                _blend_over(src_rgba8,
                            opacity,
                            &p_rgba_inout[((size_t)doc_y * p_document_width + (size_t)doc_x) * 4]);
            }
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Rasterize the layer's SVG content with lunasvg and blend the result over the document
    // ---------------------------------------------------------------------------------------------------------------------
    void Layer::_compose_vector_layer(unsigned int p_document_width, unsigned int p_document_height, double p_document_dpi, uint8_t *p_rgba_inout) const
    {
        if (svg_content.empty())
        {
            return;
        }

        std::unique_ptr<lunasvg::Document> svg_document = lunasvg::Document::loadFromData((const char *)svg_content.data(), svg_content.size());
        if (!svg_document)
        {
            fprintf(stderr, "ERROR: Vector layer '%s' contains SVG content that could not be parsed.\n", name.c_str());
            return;
        }

        /* SVG lengths are authored against SVG's own 96 DPI, so a document saved at a different */
        /* resolution has to be scaled or the shapes come out at the wrong size. */
        const double scale = p_document_dpi / 96.0;
        const int svg_width = (int)(svg_document->width() * scale);
        const int svg_height = (int)(svg_document->height() * scale);
        if (svg_width <= 0 || svg_height <= 0)
        {
            fprintf(stderr, "ERROR: Vector layer '%s' has no rasterizable size at %g DPI.\n", name.c_str(), p_document_dpi);
            return;
        }

        lunasvg::Bitmap bitmap = svg_document->renderToBitmap(svg_width, svg_height);
        if (bitmap.isNull())
        {
            fprintf(stderr, "ERROR: Vector layer '%s' could not be rasterized.\n", name.c_str());
            return;
        }

        /* lunasvg renders to ARGB32 premultiplied; this converts in place to the plain RGBA we blend. */
        bitmap.convertToRGBA();

        const uint8_t *svg_data = bitmap.data();
        const int stride = bitmap.stride();

        for (int sy = 0; sy < bitmap.height(); sy++)
        {
            for (int sx = 0; sx < bitmap.width(); sx++)
            {
                const int64_t doc_x = (int64_t)x + sx;
                const int64_t doc_y = (int64_t)y + sy;

                if (doc_x < 0 || doc_x >= (int64_t)p_document_width ||
                    doc_y < 0 || doc_y >= (int64_t)p_document_height)
                {
                    continue;
                }

                _blend_over(&svg_data[(size_t)sy * stride + (size_t)sx * 4],
                            opacity,
                            &p_rgba_inout[((size_t)doc_y * p_document_width + (size_t)doc_x) * 4]);
            }
        }
    }
};
