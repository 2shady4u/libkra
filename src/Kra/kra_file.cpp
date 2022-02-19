#include "kra_file.h"

// ---------------------------------------------------------------------------------------------------------------------
// Create a KRA Document which contains document properties and a vector of KraLayer-pointers.
// ---------------------------------------------------------------------------------------------------------------------
void KraFile::load(const std::wstring &p_path)
{
    /* Convert wstring to string */
    std::string sFilename = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(p_path);
    const char *path = sFilename.c_str();

    /* Open the KRA archive using minizip */
    unzFile file = unzOpen(path);
    if (file == NULL)
    {
        printf("(Parsing Document) ERROR: Failed to open KRA archive.\n");
        return;
    }

    /* A 'maindoc.xml' file should always be present in the KRA archive, otherwise abort immediately */
    int errorCode = unzLocateFile(file, "maindoc.xml", 1);
    if (errorCode == UNZ_OK)
    {
        printf("(Parsing Document) Found 'maindoc.xml', extracting document and layer properties\n");
    }
    else
    {
        printf("(Parsing Document) WARNING: KRA archive did not contain maindoc.xml!\n");
        return;
    }

    std::vector<unsigned char> result_vector;
    kra::extract_current_file_to_vector(file, result_vector);
    /* Put the vector into a string and parse it using tinyXML2 */
    std::string xmlString(result_vector.begin(), result_vector.end());
    tinyxml2::XMLDocument xml_document;
    xml_document.Parse(xmlString.c_str());
    tinyxml2::XMLElement *xml_element = xml_document.FirstChildElement("DOC")->FirstChildElement("IMAGE");

    /* Get important document attributes from the XML-file */
    width = xml_element->UnsignedAttribute("width", 0);
    height = xml_element->UnsignedAttribute("height", 0);
    name = xml_element->Attribute("name");
    const char *colorSpaceName = xml_element->Attribute("colorspacename");
    /* The color space defines the number of 'channels' */
    /* Each separate layer has its own color space in KRA, so not sure if this necessary... */
    if (strcmp(colorSpaceName, "RGBA") == 0)
    {
        channel_count = 4u;
    }
    else if (strcmp(colorSpaceName, "RGB") == 0)
    {
        channel_count = 3u;
    }
    else
    {
        channel_count = 0u;
    }
    printf("(Parsing Document) Document properties are extracted and have following values:\n");
    printf("(Parsing Document)  	>> name = %s\n", name.c_str());
    printf("(Parsing Document)  	>> width = %i\n", width);
    printf("(Parsing Document)  	>> height = %i\n", height);
    printf("(Parsing Document)  	>> channel_count = %i\n", channel_count);

    /* Parse all the layers registered in the maindoc.xml and add them to the document */
    layers = _parse_layers(file, xml_element);

    errorCode = unzClose(file);
}

// ---------------------------------------------------------------------------------------------------------------------
// Take a single layer and compose its tiles into something that can be used externally
// ---------------------------------------------------------------------------------------------------------------------
std::unique_ptr<KraExportedLayer> KraFile::get_exported_layer_at(int p_layer_index)
{
    std::unique_ptr<KraExportedLayer> exported_layer = std::make_unique<KraExportedLayer>();

    if (p_layer_index < 0 || p_layer_index >= layers.size())
    {
        // There should be some kind of error here
    }
    else
    {
        auto const &layer = layers.at(p_layer_index);
        exported_layer = layer->get_exported_layer();
    }

    return exported_layer;
}

// ---------------------------------------------------------------------------------------------------------------------
// Take all the layers and their tiles and construct/compose the complete image!
// ---------------------------------------------------------------------------------------------------------------------
std::vector<std::unique_ptr<KraExportedLayer>> KraFile::get_all_exported_layers()
{

    std::vector<std::unique_ptr<KraExportedLayer>> exportedLayers;

    /* Go through all the layers and add them to the exportedLayers vector */
    for (auto const &layer : layers)
    {
        std::unique_ptr<KraExportedLayer> exported_layer = std::make_unique<KraExportedLayer>();
        exported_layer = layer->get_exported_layer();

        exportedLayers.push_back(std::move(exported_layer));
    }

    /* Reverse the direction of the vector as to preserve draw order */
    std::reverse(exportedLayers.begin(), exportedLayers.end());
    return exportedLayers;
}

// ---------------------------------------------------------------------------------------------------------------------
// Get an exported version of the exact layer with the given uuid
// ---------------------------------------------------------------------------------------------------------------------
std::unique_ptr<KraExportedLayer> KraFile::get_exported_layer_with_uuid(std::string p_uuid)
{
    std::unique_ptr<KraExportedLayer> exported_layer = std::make_unique<KraExportedLayer>();



    return exported_layer;
}

// ---------------------------------------------------------------------------------------------------------------------
// Go through the XML-file and extract all the layer properties.
// ---------------------------------------------------------------------------------------------------------------------
std::vector<std::unique_ptr<KraLayer>> KraFile::_parse_layers(unzFile p_file, tinyxml2::XMLElement *xmlElement)
{
    std::vector<std::unique_ptr<KraLayer>> layers;
    const tinyxml2::XMLElement *layers_element = xmlElement->FirstChildElement("layers");
    const tinyxml2::XMLElement *layer_node = layers_element->FirstChild()->ToElement();

    /* Hopefully we find something... otherwise there are no layers! */
    /* Keep trying to find a layer until we can't find any new ones! */
    while (layer_node != 0)
    {
        /* Check the type of the layer and proceed from there... */
        std::string node_type = layer_node->Attribute("nodetype");
        std::unique_ptr<KraLayer> layer;
        if (node_type == "paintlayer" || node_type == "grouplayer")
        {
            if (node_type == "paintlayer")
            {
                layer = std::make_unique<KraPaintLayer>();
            }
            else
            {
                layer = std::make_unique<KraGroupLayer>();
            }

            layer->import_attributes(p_file, layer_node);

            if (kra::verbosity_level == kra::VERBOSE)
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