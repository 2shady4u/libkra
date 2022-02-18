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
    printf("(Parsing Document)  	>> width = %i\n", width);
    printf("(Parsing Document)  	>> height = %i\n", height);
    printf("(Parsing Document)  	>> name = %s\n", name);
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
                layer->tiles = _parse_tiles(layerContent);
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

        const char *colorSpaceName = layer_node->Attribute("colorspacename");
        /* The color space defines the number of 'channels' */
        /* Each seperate layer has its own color space in KRA, so not sure if this necessary... */
        if (strcmp(colorSpaceName, "RGBA") == 0)
        {
            layer->channel_count = 4u;
        }
        else if (strcmp(colorSpaceName, "RGB") == 0)
        {
            layer->channel_count = 3u;
        }
        else
        {
            layer->channel_count = 0u;
        }

        printf("(Parsing Document) Layer '%s' properties are extracted and have following values:\n", layer->name.c_str());
        printf("(Parsing Document)  	>> name = %s\n", layer->name.c_str());
        printf("(Parsing Document)  	>> filename = %s\n", layer->filename.c_str());
        printf("(Parsing Document)  	>> uuid = %s\n", layer->uuid.c_str());
        printf("(Parsing Document)  	>> channel_count = %i\n", layer->channel_count);
        printf("(Parsing Document)  	>> opacity = %i\n", layer->opacity);
        printf("(Parsing Document)  	>> type = %i\n", layer->get_type());
        printf("(Parsing Document)  	>> visible = %s\n", layer->visible ? "true" : "false");
        printf("(Parsing Document)  	>> x = %i\n", layer->x);
        printf("(Parsing Document)  	>> y = %i\n", layer->y);

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
// Extract the tile data and properties from the raw binary data.
// ---------------------------------------------------------------------------------------------------------------------
std::vector<std::unique_ptr<KraTile>> KraFile::_parse_tiles(std::vector<unsigned char> layerContent)
{

    std::vector<std::unique_ptr<KraTile>> tiles;

    /* This code works with a global pointer index that gets incremented depending on element size */
    /* currentIndex obviously starts at zero and will be passed by reference */
    unsigned int currentIndex = 0;

    /* Extract the main header from the tiles */
    unsigned int version = _parse_header_element(layerContent, "VERSION ", currentIndex);
    unsigned int tileWidth = _parse_header_element(layerContent, "TILEWIDTH ", currentIndex);
    unsigned int tileHeight = _parse_header_element(layerContent, "TILEHEIGHT ", currentIndex);
    unsigned int pixelSize = _parse_header_element(layerContent, "PIXELSIZE ", currentIndex);
    unsigned int decompressed_length = pixelSize * tileWidth * tileHeight;

    printf("(Parsing Document) Tile properties (Main Header) are extracted and have following values:\n");
    printf("(Parsing Document)  	>> version = %i\n", version);
    printf("(Parsing Document)  	>> tileWidth = %i\n", tileWidth);
    printf("(Parsing Document)  	>> tileHeight = %i\n", tileHeight);
    printf("(Parsing Document)  	>> pixelSize = %i\n", pixelSize);
    printf("(Parsing Document)  	>> decompressed_length = %i\n", decompressed_length);

    unsigned int numberOfTiles = _parse_header_element(layerContent, "DATA ", currentIndex);

    for (unsigned int i = 0; i < numberOfTiles; i++)
    {
        std::unique_ptr<KraTile> tile = std::make_unique<KraTile>();
        tile->version = version;
        tile->tile_width = tileWidth;
        tile->tile_height = tileHeight;
        tile->pixel_size = pixelSize;
        tile->decompressed_length = decompressed_length;

        /* Now it is time to extract & decompress the data */
        /* First the non-general element of the header needs to be extracted */
        std::string headerString = _get_header_element(layerContent, currentIndex);
        std::regex e("(-?\\d*),(-?\\d*),(\\w*),(\\d*)");
        std::smatch sm;
        std::regex_match(headerString, sm, e);

        /* This header contains: */
        /* 1. A number that defines the left position of the tile (CAN BE NEGATIVE!!!) */
        /* 2. A number that defines the top position of the tile (CAN BE NEGATIVE!!!) */
        /* 3. The string "LZF" which states the compression algorithm */
        /* 4. A number that defines the number of bytes of the data that comes after this header. */

        /* sm[0] is the full string match which we don't need */
        /* The left position of the tile */
        std::stringstream strValue;
        strValue << sm[1];
        strValue >> tile->left;
        /* The top position of the tile */
        strValue.clear();
        strValue << sm[2];
        strValue >> tile->top;
        /* We don't really care about sm[3] since it is always 'LZF' */
        /* The number of compressed bytes coming after this header */
        strValue.clear();
        strValue << sm[4];
        strValue >> tile->compressed_length;

        /* Put all the data in a vector */
        std::vector<unsigned char> dataVector(layerContent.begin() + currentIndex, layerContent.begin() + currentIndex + tile->compressed_length);
        const uint8_t *dataVectorPointer = dataVector.data();
        /* Allocate memory for the output */
        std::unique_ptr<uint8_t[]> output = std::make_unique<uint8_t[]>(tile->decompressed_length);

        /* Now... the first byte of this dataVector is actually an indicator of compression */
        /* As follows: */
        /* 0 -> No compression, the data is actually raw! */
        /* 1 -> The data was compressed using LZF */
        /* TODO: Actually implement a check to see this byte!!! */
        _lzff_decompress(dataVectorPointer + 1, tile->compressed_length, output.get(), tile->decompressed_length);

        /* TODO: Krita might also normalize the colors in some way */
        /* This needs to be check and if present, the colors need to be denormalized */

        /* Data is saved in following format: */
        /* - Firstly all the RED data */
        /* - Secondly all the GREEN data */
        /* - Thirdly all the BLUE data */
        /* - Fourthly all the ALPHA data */
        /* This is different from the required ImageMagick format which requires quartets of: */
        /* R1, G1, B1, A1, R2, G2, ... etc */
        /* We'll just sort this here!*/
        /* TODO: Sometimes there won't be any alpha channel when it is RGB instead of RGBA. */
        std::unique_ptr<uint8_t[]> sortedOutput = std::make_unique<uint8_t[]>(tile->decompressed_length);
        int jj = 0;
        int tileArea = tile->tile_height * tile->tile_width;
        for (int i = 0; i < tileArea; i++)
        {
            sortedOutput[jj] = output[2 * tileArea + i];     // RED CHANNEL
            sortedOutput[jj + 1] = output[tileArea + i];     // GREEN CHANNEL
            sortedOutput[jj + 2] = output[i];                // BLUE CHANNEL
            sortedOutput[jj + 3] = output[3 * tileArea + i]; // ALPHA CHANNEL
            jj = jj + 4;
        }
        tile->data = std::move(sortedOutput);
        /* Q: Why are the RED and BLUE channels swapped? */
        /* A: I have no clue... that's how it is saved in the tile! */

        printf("(Parsing Document) Tile properties (Tile Header) are extracted and have following values:\n");
        printf("(Parsing Document)  	>> left = %i\n", tile->left);
        printf("(Parsing Document)  	>> top = %i\n", tile->top);
        printf("(Parsing Document)  	>> compressed_length = %i\n", tile->compressed_length);
        printf("(Parsing Document)  	>> DATA START\n");
        int j = 0;
        for (int i = 0; i < 256; i++)
        {
            j++;
            printf("%i ", tile->data.get()[i]);
            if (j == 64)
            {
                j = 0;
                printf("\n");
            }
        }
        printf("(Parsing Document)  	>> DATA END\n");

        /* Add the compressedLength to the currentIndex so the next tile starts at the correct position */
        // Needs to be done BEFORE moving the pointer's ownership to the vector!
        currentIndex += tile->compressed_length;

        tiles.push_back(std::move(tile));
    }

    return tiles;
}

// ---------------------------------------------------------------------------------------------------------------------
// Extract a header element and match it with the element name.
// ---------------------------------------------------------------------------------------------------------------------
unsigned int KraFile::_parse_header_element(std::vector<unsigned char> layerContent, const std::string &elementName, unsigned int &currentIndex)
{
    unsigned int elementIntValue = -1;
    /* First extract the header element */
    std::string elementValue = _get_header_element(layerContent, currentIndex);
    /* Try to match the elementValue string */
    if (elementValue.find(elementName) != std::string::npos)
    {
        size_t pos = elementValue.find(elementName);
        /* If found then erase it from string */
        elementValue.erase(pos, elementName.length());
        /* Dump it into the output variable using stringstream */
        std::stringstream strValue;
        strValue << elementValue;
        strValue >> elementIntValue;
    }
    else
    {
        printf("(Parsing Document) WARNING: Missing header element in tile with name '%s'\n", elementName.c_str());
    }
    return elementIntValue;
}

// ---------------------------------------------------------------------------------------------------------------------
// Extract a header element starting from the currentIndex until the next "0x0A".
// ---------------------------------------------------------------------------------------------------------------------
std::string KraFile::_get_header_element(std::vector<unsigned char> layerContent, unsigned int &currentIndex)
{
    unsigned int startIndex = currentIndex;
    /* Just go through the vector until you encounter "0x0A" (= the hex value of Line Feed) */
    while (layerContent.at(currentIndex) != (char)0x0A)
    {
        currentIndex++;
    }
    unsigned int endIndex = currentIndex;
    /* Extract this header element */
    std::vector<unsigned char> elementContent(layerContent.begin() + startIndex, layerContent.begin() + endIndex);
    /* Print it... should be disabled at some point */
    for (std::vector<int>::size_type i = 0; i < elementContent.size(); i++)
    {
        std::cout << elementContent.at(i) << ' ';
    }
    std::cout << "\n";
    std::string elementValue(reinterpret_cast<char *>(elementContent.data()), endIndex - startIndex);
    /* Increment the currentIndex pointer so that we skip the "0x0A" character */
    currentIndex++;

    return elementValue;
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

// ---------------------------------------------------------------------------------------------------------------------
// Decompression function for LZF copied directly (with minor modifications) from the Krita codebase (libs\image\tiles3\swap\kis_lzf_compression.cpp).
// ---------------------------------------------------------------------------------------------------------------------

int KraFile::_lzff_decompress(const void *input, int length, void *output, int maxout)
{
    const unsigned char *ip = (const unsigned char *)input;
    const unsigned char *ip_limit = ip + length - 1;
    unsigned char *op = (unsigned char *)output;
    unsigned char *op_limit = op + maxout;
    unsigned char *ref;

    while (ip < ip_limit)
    {
        unsigned int ctrl = (*ip) + 1;
        unsigned int ofs = ((*ip) & 31) << 8;
        unsigned int len = (*ip++) >> 5;

        if (ctrl < 33)
        {
            /* literal copy */
            if (op + ctrl > op_limit)
                return 0;

            /* crazy unrolling */
            if (ctrl)
            {
                *op++ = *ip++;
                ctrl--;

                if (ctrl)
                {
                    *op++ = *ip++;
                    ctrl--;

                    if (ctrl)
                    {
                        *op++ = *ip++;
                        ctrl--;

                        for (; ctrl; ctrl--)
                            *op++ = *ip++;
                    }
                }
            }
        }
        else
        {
            /* back reference */
            len--;
            ref = op - ofs;
            ref--;

            if (len == 7 - 1)
                len += *ip++;

            ref -= *ip++;

            if (op + len + 3 > op_limit)
                return 0;

            if (ref < (unsigned char *)output)
                return 0;

            *op++ = *ref++;
            *op++ = *ref++;
            *op++ = *ref++;
            if (len)
                for (; len; --len)
                    *op++ = *ref++;
        }
    }

    return (int)(op - (unsigned char *)output);
}