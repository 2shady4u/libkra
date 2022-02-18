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
    unzFile m_zf = unzOpen(path);
    if (m_zf == NULL)
    {
        printf("(Parsing Document) ERROR: Failed to open KRA archive.\n");
        return;
    }

    /* A 'maindoc.xml' file should always be present in the KRA archive, otherwise abort immediately */
    int errorCode = unzLocateFile(m_zf, "maindoc.xml", 1);
    if (errorCode == UNZ_OK)
    {
        printf("(Parsing Document) Found 'maindoc.xml', extracting document and layer properties\n");
    }
    else
    {
        printf("(Parsing Document) WARNING: KRA archive did not contain maindoc.xml!\n");
        return;
    }

    std::vector<unsigned char> resultVector;
    _extract_current_file_to_vector(resultVector, m_zf);
    /* Put the vector into a string and parse it using tinyXML2 */
    std::string xmlString(resultVector.begin(), resultVector.end());
    tinyxml2::XMLDocument xmlDocument;
    xmlDocument.Parse(xmlString.c_str());
    tinyxml2::XMLElement *xmlElement = xmlDocument.FirstChildElement("DOC")->FirstChildElement("IMAGE");

    /* Get important document attributes from the XML-file */
    width = xmlElement->UnsignedAttribute("width", 0);
    height = xmlElement->UnsignedAttribute("height", 0);
    name = xmlElement->Attribute("name");
    const char *colorSpaceName = xmlElement->Attribute("colorspacename");
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
    layers = _parse_layers(xmlElement);

    /* Go through all the layers and initiate their tiles */
    /* Only layers of the type paintlayer get their tiles parsed */
    /* This also automatically decrypts the tile data */
    for (auto &layer : layers)
    {
        if (layer->get_type() == KraLayer::PAINT_LAYER)
        {
            const std::string &layerPath = (std::string)name + "/layers/" + (std::string)layer->filename;
            std::vector<unsigned char> layerContent;
            const char *cLayerPath = layerPath.c_str();
            int errorCode = unzLocateFile(m_zf, cLayerPath, 1);
            errorCode += _extract_current_file_to_vector(layerContent, m_zf);
            if (errorCode == UNZ_OK)
            {
                /* Start extracting the tile data. */
                layer->parse_tiles(layerContent);
            }
            else
            {
                printf("(Parsing Document) WARNING: Layer entry with path '%s' could not be found in KRA archive.\n", layerPath.c_str());
            }
        }
    }

    errorCode = unzClose(m_zf);
}

// ---------------------------------------------------------------------------------------------------------------------
// Take a single layer and compose its tiles into something that can be used externally
// ---------------------------------------------------------------------------------------------------------------------
std::unique_ptr<KraExportedLayer> KraFile::get_exported_layer(int p_layer_index)
{
    std::unique_ptr<KraExportedLayer> exported_layer = std::make_unique<KraExportedLayer>();

    if (p_layer_index < 0 || p_layer_index >= layers.size())
    {
        // There should be some kind of error here
    }
    else
    {
        auto const &layer = layers.at(p_layer_index);
        /* Copy all important properties immediately */
        exported_layer->name = layer->name;
        exported_layer->channel_count = layer->channel_count;
        exported_layer->x = layer->x;
        exported_layer->y = layer->y;
        exported_layer->opacity = layer->opacity;
        exported_layer->visible = layer->visible;

        /*Initialize the extents of this layer to 0 */
        exported_layer->left = 0;
        exported_layer->right = 0;
        exported_layer->top = 0;
        exported_layer->bottom = 0;

        /* find the extents of the layer canvas */
        for (auto const &tile : layer->tiles)
        {
            if (tile->left < exported_layer->left)
            {
                exported_layer->left = tile->left;
            }
            if (tile->left + (int32_t)tile->tile_width > exported_layer->right)
            {
                exported_layer->right = tile->left + (int32_t)tile->tile_width;
            }

            if (tile->top < exported_layer->top)
            {
                exported_layer->top = tile->top;
            }
            if (tile->top + (int32_t)tile->tile_height > exported_layer->bottom)
            {
                exported_layer->bottom = tile->top + (int32_t)tile->tile_height;
            }
        }
        unsigned int layerHeight = (unsigned int)(exported_layer->bottom - exported_layer->top);
        unsigned int layerWidth = (unsigned int)(exported_layer->right - exported_layer->left);

        if (layer->tiles.size() == 0)
        {
            printf("(Exporting Document) Exported Layer '%s' is empty... skipping!\n", exported_layer->name.c_str());
        }
        else
        {
            /* Get a reference tile and extract the number of horizontal and vertical tiles */
            std::unique_ptr<KraTile> &referenceTile = layer->tiles[0];
            unsigned int numberOfColumns = layerWidth / referenceTile->tile_width;
            unsigned int numberOfRows = layerHeight / referenceTile->tile_height;
            size_t composedDataSize = numberOfColumns * numberOfRows * referenceTile->decompressed_length;

            printf("(Exporting Document) Exported Layer '%s' properties are extracted and have following values:\n", exported_layer->name.c_str());
            printf("(Exporting Document)  	>> numberOfColumns = %i\n", numberOfColumns);
            printf("(Exporting Document)  	>> numberOfRows = %i\n", numberOfRows);
            printf("(Exporting Document)  	>> layerWidth = %i\n", layerWidth);
            printf("(Exporting Document)  	>> layerHeight = %i\n", layerHeight);
            printf("(Exporting Document)  	>> top = %i\n", exported_layer->top);
            printf("(Exporting Document)  	>> bottom = %i\n", exported_layer->bottom);
            printf("(Exporting Document)  	>> left = %i\n", exported_layer->left);
            printf("(Exporting Document)  	>> right = %i\n", exported_layer->right);
            printf("(Exporting Document)  	>> composedDataSize = %i\n", static_cast<int>(composedDataSize));

            /* Allocate space for the output data! */

            std::unique_ptr<uint8_t[]> composedData = std::make_unique<uint8_t[]>(composedDataSize);
            /* I initialize all these elements to zero to avoid empty tiles from being filled with junk */
            /* Problem might be that this takes quite a lot of time... */
            /* TODO: Only the empty tiles should be initialized to zero! */

            /* IMPORTANT: Not all the tiles exist! */
            /* Empty tiles (containing full ALPHA) are not added as tiles! */
            /* Now we have to construct the data in such a way that all tiles are in the correct positions */
            for (auto const &tile : layer->tiles)
            {
                int currentNormalizedTop = tile->top - exported_layer->top;
                int currentNormalizedLeft = tile->left - exported_layer->left;
                for (int rowIndex = 0; rowIndex < (int)tile->tile_height; rowIndex++)
                {
                    uint8_t *destination = composedData.get() + tile->pixel_size * tile->tile_width * rowIndex * numberOfColumns;
                    destination += tile->pixel_size * currentNormalizedLeft;
                    destination += tile->pixel_size * tile->tile_width * currentNormalizedTop * numberOfColumns;
                    uint8_t *source = tile->data.get() + tile->pixel_size * tile->tile_width * rowIndex;
                    size_t size = tile->pixel_size * tile->tile_width;
                    /* Copy the row of the tile to the composed image */
                    std::memcpy(destination, source, size);
                }
            }
            exported_layer->data = std::move(composedData);
        }
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
        if (layer->get_type() != KraLayer::PAINT_LAYER)
        {
            printf("(Exporting Document) Ignoring non-exportable Layer '%s'\n", layer->name.c_str());
        }
        else
        {
            std::unique_ptr<KraExportedLayer> exportedLayer = std::make_unique<KraExportedLayer>();
            /* Copy all important properties immediately */
            exportedLayer->name = layer->name;
            exportedLayer->channel_count = layer->channel_count;
            exportedLayer->x = layer->x;
            exportedLayer->y = layer->y;
            exportedLayer->opacity = layer->opacity;
            exportedLayer->visible = layer->visible;

            /*Initialize the extents of this layer to 0 */
            exportedLayer->left = 0;
            exportedLayer->right = 0;
            exportedLayer->top = 0;
            exportedLayer->bottom = 0;

            /* find the extents of the layer canvas */
            for (auto const &tile : layer->tiles)
            {
                if (tile->left < exportedLayer->left)
                {
                    exportedLayer->left = tile->left;
                }
                if (tile->left + (int32_t)tile->tile_width > exportedLayer->right)
                {
                    exportedLayer->right = tile->left + (int32_t)tile->tile_width;
                }

                if (tile->top < exportedLayer->top)
                {
                    exportedLayer->top = tile->top;
                }
                if (tile->top + (int32_t)tile->tile_height > exportedLayer->bottom)
                {
                    exportedLayer->bottom = tile->top + (int32_t)tile->tile_height;
                }
            }
            unsigned int layerHeight = (unsigned int)(exportedLayer->bottom - exportedLayer->top);
            unsigned int layerWidth = (unsigned int)(exportedLayer->right - exportedLayer->left);

            if (layer->tiles.size() == 0)
            {
                printf("(Exporting Document) Exported Layer '%s' is empty... skipping!\n", exportedLayer->name.c_str());
                continue;
            }
            /* Get a reference tile and extract the number of horizontal and vertical tiles */
            std::unique_ptr<KraTile> &referenceTile = layer->tiles[0];
            unsigned int numberOfColumns = layerWidth / referenceTile->tile_width;
            unsigned int numberOfRows = layerHeight / referenceTile->tile_height;
            size_t composedDataSize = numberOfColumns * numberOfRows * referenceTile->decompressed_length;

            printf("(Exporting Document) Exported Layer '%s' properties are extracted and have following values:\n", exportedLayer->name.c_str());
            printf("(Exporting Document)  	>> numberOfColumns = %i\n", numberOfColumns);
            printf("(Exporting Document)  	>> numberOfRows = %i\n", numberOfRows);
            printf("(Exporting Document)  	>> layerWidth = %i\n", layerWidth);
            printf("(Exporting Document)  	>> layerHeight = %i\n", layerHeight);
            printf("(Exporting Document)  	>> top = %i\n", exportedLayer->top);
            printf("(Exporting Document)  	>> bottom = %i\n", exportedLayer->bottom);
            printf("(Exporting Document)  	>> left = %i\n", exportedLayer->left);
            printf("(Exporting Document)  	>> right = %i\n", exportedLayer->right);
            printf("(Exporting Document)  	>> composedDataSize = %i\n", static_cast<int>(composedDataSize));

            /* Allocate space for the output data! */

            std::unique_ptr<uint8_t[]> composedData = std::make_unique<uint8_t[]>(composedDataSize);
            /* I initialize all these elements to zero to avoid empty tiles from being filled with junk */
            /* Problem might be that this takes quite a lot of time... */
            /* TODO: Only the empty tiles should be initialized to zero! */

            /* IMPORTANT: Not all the tiles exist! */
            /* Empty tiles (containing full ALPHA) are not added as tiles! */
            /* Now we have to construct the data in such a way that all tiles are in the correct positions */
            for (auto const &tile : layer->tiles)
            {
                int currentNormalizedTop = tile->top - exportedLayer->top;
                int currentNormalizedLeft = tile->left - exportedLayer->left;
                for (int rowIndex = 0; rowIndex < (int)tile->tile_height; rowIndex++)
                {
                    uint8_t *destination = composedData.get() + tile->pixel_size * tile->tile_width * rowIndex * numberOfColumns;
                    destination += tile->pixel_size * currentNormalizedLeft;
                    destination += tile->pixel_size * tile->tile_width * currentNormalizedTop * numberOfColumns;
                    uint8_t *source = tile->data.get() + tile->pixel_size * tile->tile_width * rowIndex;
                    size_t size = tile->pixel_size * tile->tile_width;
                    /* Copy the row of the tile to the composed image */
                    std::memcpy(destination, source, size);
                }
            }
            exportedLayer->data = std::move(composedData);
            exportedLayers.push_back(std::move(exportedLayer));
        }
    }
    /* Reverse the direction of the vector, so that order is preserved in the Godot mirror universe */
    std::reverse(exportedLayers.begin(), exportedLayers.end());
    return exportedLayers;
}

// ---------------------------------------------------------------------------------------------------------------------
// Go through the XML-file and extract all the layer properties.
// ---------------------------------------------------------------------------------------------------------------------
std::vector<std::unique_ptr<KraLayer>> KraFile::_parse_layers(tinyxml2::XMLElement *xmlElement)
{
    std::vector<std::unique_ptr<KraLayer>> layers;
    const tinyxml2::XMLElement *layers_element = xmlElement->FirstChildElement("layers");
    const tinyxml2::XMLElement *layer_node = layers_element->FirstChild()->ToElement();

    /* Hopefully we find something... otherwise there are no layers! */
    /* Keep trying to find a layer until we can't any new ones! */
    while (layer_node != 0)
    {
        /* Check the type of the layer and proceed from there... */
        const char *node_type = layer_node->Attribute("nodetype");
        KraLayer::KraLayerType type;
        if (strcmp(node_type, "paintlayer") == 0)
        {
            type = KraLayer::PAINT_LAYER;
        }
        else if (strcmp(node_type, "grouplayer") == 0)
        {
            type = KraLayer::GROUP_LAYER;
        }

        std::unique_ptr<KraLayer> layer = std::make_unique<KraPaintLayer>();
        layer->import_attributes(layer_node);

        if (verbosity_level == VERBOSE)
        {
            layer->print_layer_attributes();
        }

        layers.push_back(std::move(layer));

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
// Extract the data content of the current file in the ZIP archive to a vector.
// ---------------------------------------------------------------------------------------------------------------------

int KraFile::_extract_current_file_to_vector(std::vector<unsigned char> &resultVector, unzFile &m_zf)
{
    size_t errorCode = UNZ_ERRNO;
    unz_file_info64 file_info = {0};
    char filename_inzip[256] = {0};

    /* Get the required size necessary to store the file content. */
    errorCode = unzGetCurrentFileInfo64(m_zf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
    size_t uncompressed_size = file_info.uncompressed_size;

    errorCode = unzOpenCurrentFile(m_zf);

    std::vector<unsigned char> buffer;
    buffer.resize(WRITEBUFFERSIZE);
    resultVector.reserve((size_t)file_info.uncompressed_size);

    /* error_code serves also as the number of bytes that were read... */
    do
    {
        /* Read the data in parts of size WRITEBUFFERSIZE */
        /* and keep reading until the function returns zero or lower. */
        errorCode = unzReadCurrentFile(m_zf, buffer.data(), (unsigned int)buffer.size());
        if (errorCode < 0 || errorCode == 0)
            break;

        resultVector.insert(resultVector.end(), buffer.data(), buffer.data() + errorCode);

    } while (errorCode > 0);

    /* Be sure to close the file to avoid leakage. */
    errorCode = unzCloseCurrentFile(m_zf);

    return (int)errorCode;
}