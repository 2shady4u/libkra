#include "kra_layer.h"

namespace kra
{
    void KraLayer::import_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
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
        }
    }

    void KraLayer::_import_paint_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
    {
        const char *color_space_name = p_xml_element->Attribute("colorspacename");
        /* The color space defines the number of 'channels' */
        /* Each seperate layer can have its own color space in KRA, but this doesn't seem to used by default */
        if (strcmp(color_space_name, "RGBA") == 0)
        {
            color_space = RGBA;
        }
        else if (strcmp(color_space_name, "CMYK") == 0)
        {
            color_space = CMYK;
        }

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
        }
        else
        {
            printf("(Parsing Document) WARNING: Layer entry with path '%s' could not be found in KRA archive.\n", layer_path.c_str());
        }
    }

    void KraLayer::_import_group_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
    {
        const tinyxml2::XMLElement *layers_element = p_xml_element->FirstChildElement("layers");
        const tinyxml2::XMLElement *layer_node = layers_element->FirstChild()->ToElement();

        while (layer_node != 0)
        {
            /* Check the type of the layer and proceed from there... */
            std::string node_type = layer_node->Attribute("nodetype");
            std::unique_ptr<KraLayer> layer = std::make_unique<KraLayer>();
            if (node_type == "paintlayer" || node_type == "grouplayer")
            {
                if (node_type == "paintlayer")
                {
                    layer->type = PAINT_LAYER;
                }
                else
                {
                    layer->type = GROUP_LAYER;
                }

                layer->import_attributes(p_name, p_file, layer_node);

                if (verbosity_level == VERBOSE)
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

    void KraLayer::print_layer_attributes() const
    {
        printf("(Parsing Document) Layer '%s' properties are extracted and have following values:\n", name.c_str());
        printf("(Parsing Document)  	>> filename = %s\n", filename.c_str());
        printf("(Parsing Document)  	>> name = %s\n", name.c_str());
        printf("(Parsing Document)  	>> uuid = %s\n", uuid.c_str());
        printf("(Parsing Document)  	>> color_space = %i\n", color_space);
        printf("(Parsing Document)  	>> x = %i\n", x);
        printf("(Parsing Document)  	>> y = %i\n", y);
        printf("(Parsing Document)  	>> opacity = %i\n", opacity);
        printf("(Parsing Document)  	>> visible = %s\n", visible ? "true" : "false");
        printf("(Parsing Document)  	>> type = %i\n", type);

        switch (type)
        {
        case PAINT_LAYER:
            break;
        case GROUP_LAYER:
            _print_group_layer_attributes();
            break;
        }
    }

    void KraLayer::_print_group_layer_attributes() const
    {
        printf("(Parsing Document)  	>> my children are:\n");
        for (const auto &layer : children)
        {
            printf("(Parsing Document)  	>> '%s'\n", layer->name.c_str());
        }
    }

    std::unique_ptr<KraExportedLayer> KraLayer::get_exported_layer()
    {
        std::unique_ptr<KraExportedLayer> exported_layer = std::make_unique<KraExportedLayer>();

        /* Copy all important properties immediately */
        exported_layer->name = name;
        exported_layer->color_space = color_space;
        exported_layer->x = x;
        exported_layer->y = y;
        exported_layer->opacity = opacity;
        exported_layer->visible = visible;
        exported_layer->type = type;

        switch (type)
        {
        case PAINT_LAYER:
        {
            exported_layer->top = layer_data->get_top();
            exported_layer->left = layer_data->get_left();
            exported_layer->bottom = layer_data->get_bottom();
            exported_layer->right = layer_data->get_right();

            exported_layer->pixel_size = layer_data->pixel_size;

            exported_layer->data = layer_data->get_composed_data();
            break;
        }
        case GROUP_LAYER:
            for (auto const &child : children)
            {
                exported_layer->child_uuids.push_back(child->uuid);
            }
            break;
        }

        return exported_layer;
    }
};