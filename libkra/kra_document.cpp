// ############################################################################ #
// Copyright © 2022-2024 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "kra_document.h"

namespace kra
{
    // ---------------------------------------------------------------------------------------------------------------------
    // Load a KRA/KRZ archive from file and import document properties and layers
    // ---------------------------------------------------------------------------------------------------------------------
    void Document::load(const std::wstring &p_path)
    {
        /* Convert wstring to string */
        std::string string_path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(p_path);
        const char *char_path = string_path.c_str();

        /* Open the KRA archive using zlib */
        unzFile file = unzOpen(char_path);
        if (file == NULL)
        {
            fprintf(stderr, "ERROR: Failed to open KRA/KRZ archive at path '%s'\n", char_path);
            return;
        }

        /* A 'maindoc.xml' file should always be present in the KRA/KRZ archive, if not return immediately */
        int errorCode = unzLocateFile(file, "maindoc.xml", 1);
        if (errorCode == UNZ_OK)
        {
            if (verbosity_level > QUIET)
            {
                fprintf(stdout, "Found 'maindoc.xml', extracting document and layer properties\n");
            }
        }
        else
        {
            fprintf(stderr, "ERROR: Required file 'maindoc.xml' is missing in archive ('%s')\n", char_path);
            return;
        }

        std::vector<unsigned char> result_vector;
        extract_current_file_to_vector(file, result_vector);

        /* Convert the vector into a string and parse it using tinyXML2 */
        const std::string xml_string(result_vector.begin(), result_vector.end());
        tinyxml2::XMLDocument xml_document;
        xml_document.Parse(xml_string.c_str());

        /* Get important document attributes from the XML-file */
        const tinyxml2::XMLElement *xml_element = xml_document.FirstChildElement("DOC")->FirstChildElement("IMAGE");
        width = xml_element->UnsignedAttribute("width", 0);
        height = xml_element->UnsignedAttribute("height", 0);
        name = xml_element->Attribute("name");
        std::string color_space_name = xml_element->Attribute("colorspacename");
        /* Each separate layer also has its own color space in KRA, so this color_space isn't really important */
        color_space = get_color_space(color_space_name);

        if (verbosity_level >= VERBOSE)
        {
            print_document_attributes();
        }

        /* Parse all the layers registered in the maindoc.xml and add them to the document */
        layers = _parse_layers(file, xml_element);
    
        _create_layer_map();

        /* Close the KRA/KRZ archive */
        errorCode = unzClose(file);
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Take a single layer, at a certain index, and get an exported version of this layer
    // ---------------------------------------------------------------------------------------------------------------------
    std::unique_ptr<ExportedLayer> Document::get_exported_layer_at(int p_layer_index) const
    {
        std::unique_ptr<ExportedLayer> exported_layer = std::make_unique<ExportedLayer>();

        if (p_layer_index >= 0 && p_layer_index < layers.size())
        {
            auto const &layer = layers.at(p_layer_index);
            exported_layer = layer->get_exported_layer();
        }
        else
        {
            fprintf(stderr, "ERROR: Index %i is out of range, should be between 0 and %zi", p_layer_index, layers.size());
        }

        return exported_layer;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Get an exported version of the exact layer with the given uuid
    // ---------------------------------------------------------------------------------------------------------------------
    std::unique_ptr<ExportedLayer> Document::get_exported_layer_with_uuid(const std::string &p_uuid) const
    {
        std::unique_ptr<ExportedLayer> exported_layer = std::make_unique<ExportedLayer>();

        if (layer_map.find(p_uuid) != layer_map.end())
        {
            const std::unique_ptr<Layer> &layer = layer_map.at(p_uuid);
            exported_layer = layer->get_exported_layer();
        }
        else
        {
            fprintf(stderr, "ERROR: There's no layer with UUID %s", p_uuid.c_str());
        }

        return exported_layer;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Take all the layers and their tiles and construct/compose the complete image!
    // ---------------------------------------------------------------------------------------------------------------------
    std::vector<std::unique_ptr<ExportedLayer>> Document::get_all_exported_layers() const
    {
        std::vector<std::unique_ptr<ExportedLayer>> exported_layers;

        /* Go through all the layers and add them to the exported_layers vector */
        for (auto const &layer : layers)
        {
            std::unique_ptr<ExportedLayer> exported_layer = std::make_unique<ExportedLayer>();
            exported_layer = layer->get_exported_layer();

            exported_layers.push_back(std::move(exported_layer));
        }

        /* Reverse the direction of the vector as to preserve draw order */
        std::reverse(exported_layers.begin(), exported_layers.end());
        return exported_layers;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Print document attributes to the output console
    // ---------------------------------------------------------------------------------------------------------------------
    void Document::print_document_attributes() const
    {
        fprintf(stdout, "----- Document attributes are extracted and have following values:\n");
        fprintf(stdout, "   >> name = %s\n", name.c_str());
        fprintf(stdout, "   >> width = %i\n", width);
        fprintf(stdout, "   >> height = %i\n", height);
        fprintf(stdout, "   >> color_space = %i\n", color_space);
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Go through the XML-file and extract all the layer properties.
    // ---------------------------------------------------------------------------------------------------------------------
    std::vector<std::unique_ptr<Layer>> Document::_parse_layers(unzFile &p_file, const tinyxml2::XMLElement *xmlElement)
    {
        std::vector<std::unique_ptr<Layer>> layers;
        const tinyxml2::XMLElement *layers_element = xmlElement->FirstChildElement("layers");
        const tinyxml2::XMLElement *layer_node = layers_element->FirstChild()->ToElement();
        
        if (verbosity_level >= VERBOSE)
        {
            fprintf(stdout, "Recursively extracting layer attributes and data from 'maindoc.xml'...\n");
        }

        /* Hopefully we find something... otherwise there are no layers! */
        /* Keep trying to find a layer until we can't find any new ones! */
        while (layer_node != 0)
        {
            /* Check the type of the layer and proceed from there... */
            std::string node_type = layer_node->Attribute("nodetype");
            std::unique_ptr<Layer> layer = std::make_unique<Layer>();
            /* If it is not a paintlayer nor a grouplayer then we don't support it! */
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

                layer->import_attributes(name, p_file, layer_node);

                if (verbosity_level >= VERBOSE)
                {
                    layer->print_layer_attributes();
                }

                layers.push_back(std::move(layer));
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

        return layers;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Create the layer_map as to easily find layers by their UUID
    // ---------------------------------------------------------------------------------------------------------------------
    void Document::_create_layer_map()
    {
        layer_map.clear();
        /* Go through all the layers and add them to the map */
        for (auto const &layer : layers)
        {
            _add_layer_to_map(layer);
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Recursively add all child layers (and those children's children) to the layer_map
    // ---------------------------------------------------------------------------------------------------------------------
    void Document::_add_layer_to_map(const std::unique_ptr<Layer> &layer)
    {
        layer_map.insert({layer->uuid, layer});
        /* Also add all the children of this layer to the map */
        for (auto const &child : layer->children)
        {
            _add_layer_to_map(child);
        }
    }
};