#include "kra_layer.h"
// TODO: Should be removed at some point!
#include "kra_file.h"

void KraLayer::import_attributes(unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
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
        _import_paint_attributes(p_file, p_xml_element);
        break;
    case GROUP_LAYER:
        _import_group_attributes(p_file, p_xml_element);
        break;
    }
}

void KraLayer::_import_paint_attributes(unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
{
    const char *color_space_name = p_xml_element->Attribute("colorspacename");
    /* The color space defines the number of 'channels' */
    /* Each seperate layer has its own color space in KRA, so not sure if this necessary... */
    if (strcmp(color_space_name, "RGBA") == 0)
    {
        channel_count = 4u;
    }
    else if (strcmp(color_space_name, "RGB") == 0)
    {
        channel_count = 3u;
    }
    else
    {
        channel_count = 0u;
    }

    /* Try and find the relevant file that defines this layer's tile data */
    /* This also automatically decrypts the tile data */
    /* The "Sample/"-folder is hard-coded as I have yet to encounter a case where this folder is named differently! */
    const std::string &layer_path = "Sample/layers/" + filename;
    std::vector<unsigned char> layer_content;
    const char *c_path = layer_path.c_str();
    int errorCode = unzLocateFile(p_file, c_path, 1);
    errorCode += kra::extract_current_file_to_vector(p_file, layer_content);
    if (errorCode == UNZ_OK)
    {
        /* Start extracting the tile data. */
        _parse_tiles(layer_content);
    }
    else
    {
        printf("(Parsing Document) WARNING: Layer entry with path '%s' could not be found in KRA archive.\n", layer_path.c_str());
    }
}

void KraLayer::_import_group_attributes(unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
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
                layer->type = KraLayer::PAINT_LAYER;
            }
            else
            {
                layer->type = KraLayer::GROUP_LAYER;
            }

            layer->import_attributes(p_file, layer_node);

            if (kra::verbosity_level == kra::VERBOSE)
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
    printf("(Parsing Document)  	>> channel_count = %i\n", channel_count);
    printf("(Parsing Document)  	>> x = %i\n", x);
    printf("(Parsing Document)  	>> y = %i\n", y);
    printf("(Parsing Document)  	>> opacity = %i\n", opacity);
    printf("(Parsing Document)  	>> visible = %s\n", visible ? "true" : "false");
    printf("(Parsing Document)  	>> type = %i\n", type);

    switch (type)
    {
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
    exported_layer->channel_count = channel_count;
    exported_layer->x = x;
    exported_layer->y = y;
    exported_layer->opacity = opacity;
    exported_layer->visible = visible;

    /* Initialize the extents of this layer to 0 */
    exported_layer->left = 0;
    exported_layer->right = 0;
    exported_layer->top = 0;
    exported_layer->bottom = 0;

    /* find the extents of the layer canvas */
    for (auto const &tile : tiles)
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
    unsigned int layer_width = (unsigned int)(exported_layer->right - exported_layer->left);
    unsigned int layer_height = (unsigned int)(exported_layer->bottom - exported_layer->top);

    if (tiles.size() == 0)
    {
        printf("(Exporting Document) Exported Layer '%s' is empty... skipping!\n", exported_layer->name.c_str());
    }
    else
    {
        /* Get a reference tile and extract the number of horizontal and vertical tiles */
        std::unique_ptr<KraTile> &referenceTile = tiles[0];
        unsigned int numberOfColumns = layer_width / referenceTile->tile_width;
        unsigned int numberOfRows = layer_height / referenceTile->tile_height;
        size_t composed_data_size = numberOfColumns * numberOfRows * referenceTile->decompressed_length;

        printf("(Exporting Document) Exported Layer '%s' properties are extracted and have following values:\n", exported_layer->name.c_str());
        printf("(Exporting Document)  	>> numberOfColumns = %i\n", numberOfColumns);
        printf("(Exporting Document)  	>> numberOfRows = %i\n", numberOfRows);
        printf("(Exporting Document)  	>> layerWidth = %i\n", layer_width);
        printf("(Exporting Document)  	>> layerHeight = %i\n", layer_height);
        printf("(Exporting Document)  	>> top = %i\n", exported_layer->top);
        printf("(Exporting Document)  	>> bottom = %i\n", exported_layer->bottom);
        printf("(Exporting Document)  	>> left = %i\n", exported_layer->left);
        printf("(Exporting Document)  	>> right = %i\n", exported_layer->right);
        printf("(Exporting Document)  	>> composedDataSize = %i\n", static_cast<int>(composed_data_size));

        /* Allocate space for the output data! */

        std::unique_ptr<uint8_t[]> composed_data = std::make_unique<uint8_t[]>(composed_data_size);
        /* I initialize all these elements to zero to avoid empty tiles from being filled with junk */
        /* Problem might be that this takes quite a lot of time... */
        /* TODO: Only the empty tiles should be initialized to zero! */

        /* IMPORTANT: Not all the tiles exist! */
        /* Empty tiles (containing full ALPHA) are not added as tiles! */
        /* Now we have to construct the data in such a way that all tiles are in the correct positions */
        for (auto const &tile : tiles)
        {
            int currentNormalizedTop = tile->top - exported_layer->top;
            int currentNormalizedLeft = tile->left - exported_layer->left;
            for (int rowIndex = 0; rowIndex < (int)tile->tile_height; rowIndex++)
            {
                uint8_t *destination = composed_data.get() + tile->pixel_size * tile->tile_width * rowIndex * numberOfColumns;
                destination += tile->pixel_size * currentNormalizedLeft;
                destination += tile->pixel_size * tile->tile_width * currentNormalizedTop * numberOfColumns;
                uint8_t *source = tile->data.get() + tile->pixel_size * tile->tile_width * rowIndex;
                size_t size = tile->pixel_size * tile->tile_width;
                /* Copy the row of the tile to the composed image */
                std::memcpy(destination, source, size);
            }
        }
        exported_layer->data = std::move(composed_data);
    }

    return exported_layer;
}

// ---------------------------------------------------------------------------------------------------------------------
// Extract the tile data and properties from the raw binary data.
// ---------------------------------------------------------------------------------------------------------------------
void KraLayer::_parse_tiles(std::vector<unsigned char> layerContent)
{
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
}

// ---------------------------------------------------------------------------------------------------------------------
// Extract a header element and match it with the element name.
// ---------------------------------------------------------------------------------------------------------------------
unsigned int KraLayer::_parse_header_element(std::vector<unsigned char> layerContent, const std::string &elementName, unsigned int &currentIndex)
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
std::string KraLayer::_get_header_element(std::vector<unsigned char> layerContent, unsigned int &currentIndex)
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
// Decompression function for LZF copied directly (with minor modifications) from the Krita codebase (libs\image\tiles3\swap\kis_lzf_compression.cpp).
// ---------------------------------------------------------------------------------------------------------------------

int KraLayer::_lzff_decompress(const void *input, int length, void *output, int maxout)
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