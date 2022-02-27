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
            _parse_tiles(layer_content);
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

        exported_layer->pixel_size = pixel_size;

        /* Initialize the extents of this layer to 0 */
        exported_layer->left = 0;
        exported_layer->right = 0;
        exported_layer->top = 0;
        exported_layer->bottom = 0;

        switch (type)
        {
        case PAINT_LAYER:
        {
            /* find the extents of the layer canvas */
            for (auto const &tile : tiles)
            {
                if (tile->left < exported_layer->left)
                {
                    exported_layer->left = tile->left;
                }
                if (tile->left + (int32_t)tile_width > exported_layer->right)
                {
                    exported_layer->right = tile->left + (int32_t)tile_width;
                }

                if (tile->top < exported_layer->top)
                {
                    exported_layer->top = tile->top;
                }
                if (tile->top + (int32_t)tile_height > exported_layer->bottom)
                {
                    exported_layer->bottom = tile->top + (int32_t)tile_height;
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
                std::unique_ptr<KraTile> &reference_tile = tiles[0];
                unsigned int number_of_columns = layer_width / tile_width;
                unsigned int number_of_rows = layer_height / tile_height;
                size_t composed_data_size = number_of_columns * number_of_rows * reference_tile->decompressed_length;

                printf("(Exporting Document) Exported Layer '%s' properties are extracted and have following values:\n", exported_layer->name.c_str());
                printf("(Exporting Document)  	>> number_of_columns = %i\n", number_of_columns);
                printf("(Exporting Document)  	>> number_of_rows = %i\n", number_of_rows);
                printf("(Exporting Document)  	>> layer_width = %i\n", layer_width);
                printf("(Exporting Document)  	>> layer_height = %i\n", layer_height);
                printf("(Exporting Document)  	>> top = %i\n", exported_layer->top);
                printf("(Exporting Document)  	>> bottom = %i\n", exported_layer->bottom);
                printf("(Exporting Document)  	>> left = %i\n", exported_layer->left);
                printf("(Exporting Document)  	>> right = %i\n", exported_layer->right);
                printf("(Exporting Document)  	>> composed_data_size = %i\n", static_cast<int>(composed_data_size));

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
                    int current_normalized_top = tile->top - exported_layer->top;
                    int current_normalized_left = tile->left - exported_layer->left;
                    for (int row_index = 0; row_index < (int)tile_height; row_index++)
                    {
                        uint8_t *destination = composed_data.get() + pixel_size * tile_width * row_index * number_of_columns;
                        destination += pixel_size * current_normalized_left;
                        destination += pixel_size * tile_width * current_normalized_top * number_of_columns;
                        uint8_t *source = tile->data.get() + pixel_size * tile_width * row_index;
                        size_t size = pixel_size * tile_width;
                        /* Copy the row of the tile to the composed image */
                        std::memcpy(destination, source, size);
                    }
                }
                exported_layer->data = std::move(composed_data);
            }
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

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract the tile data and properties from the raw binary data.
    // ---------------------------------------------------------------------------------------------------------------------
    void KraLayer::_parse_tiles(std::vector<unsigned char> p_layer_content)
    {
        /* This code works with a global pointer index that gets incremented depending on element size */
        /* current_index obviously starts at zero and will be passed by reference */
        unsigned int current_index = 0;

        /* Extract the main header from the tiles */
        version = _parse_header_element(p_layer_content, "VERSION ", current_index);
        tile_width = _parse_header_element(p_layer_content, "TILEWIDTH ", current_index);
        tile_height = _parse_header_element(p_layer_content, "TILEHEIGHT ", current_index);
        pixel_size = _parse_header_element(p_layer_content, "PIXELSIZE ", current_index);
        unsigned int decompressed_length = pixel_size * tile_width * tile_height;

        printf("(Parsing Document) Tile properties (Main Header) are extracted and have following values:\n");
        printf("(Parsing Document)  	>> version = %i\n", version);
        printf("(Parsing Document)  	>> tile_width = %i\n", tile_width);
        printf("(Parsing Document)  	>> tile_height = %i\n", tile_height);
        printf("(Parsing Document)  	>> pixel_size = %i\n", pixel_size);
        printf("(Parsing Document)  	>> decompressed_length = %i\n", decompressed_length);

        unsigned int number_of_tiles = _parse_header_element(p_layer_content, "DATA ", current_index);

        for (unsigned int i = 0; i < number_of_tiles; i++)
        {
            std::unique_ptr<KraTile> tile = std::make_unique<KraTile>();
            tile->decompressed_length = decompressed_length;

            /* Now it is time to extract & decompress the data */
            /* First the non-general element of the header needs to be extracted */
            std::string headerString = _get_header_element(p_layer_content, current_index);
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
            std::ssub_match base_sub_match = sm[1];
            tile->left = std::stoi(base_sub_match.str());
            /* The top position of the tile */
            base_sub_match = sm[2];
            tile->top = std::stoi(base_sub_match.str());
            /* We don't really care about sm[3] since it is always 'LZF' */
            /* The number of compressed bytes coming after this header */
            base_sub_match = sm[4];
            tile->compressed_length = std::stoi(base_sub_match.str());

            /* Put all the data in a vector */
            std::vector<unsigned char> data_vector(p_layer_content.begin() + current_index, p_layer_content.begin() + current_index + tile->compressed_length);
            const uint8_t *data_vector_pointer = data_vector.data();
            /* Allocate memory for the output */
            std::unique_ptr<uint8_t[]> output = std::make_unique<uint8_t[]>(tile->decompressed_length);

            /* Now... the first byte of this dataVector is actually an indicator of compression */
            /* As follows: */
            /* 0 -> No compression, the data is actually raw! */
            /* 1 -> The data was compressed using LZF */
            /* TODO: Actually implement a check to see this byte!!! */
            _lzff_decompress(data_vector_pointer + 1, tile->compressed_length, output.get(), tile->decompressed_length);

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
            std::unique_ptr<uint8_t[]> sorted_output = std::make_unique<uint8_t[]>(tile->decompressed_length);
            int jj = 0;
            int tile_area = tile_height * tile_width;
            for (int i = 0; i < tile_area; i++)
            {
                sorted_output[jj + 0] = output[2 * tile_area + i]; // RED CHANNEL
                sorted_output[jj + 1] = output[1 * tile_area + i]; // GREEN CHANNEL
                sorted_output[jj + 2] = output[0 * tile_area + i]; // BLUE CHANNEL
                sorted_output[jj + 3] = output[3 * tile_area + i]; // ALPHA CHANNEL
                jj = jj + 4;
            }
            tile->data = std::move(sorted_output);
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

            /* Add the compressedLength to the current_index so the next tile starts at the correct position */
            // Needs to be done BEFORE moving the pointer's ownership to the vector!
            current_index += tile->compressed_length;

            tiles.push_back(std::move(tile));
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract a header element and match it with the element name.
    // ---------------------------------------------------------------------------------------------------------------------
    unsigned int KraLayer::_parse_header_element(const std::vector<unsigned char> &p_layer_content, const std::string &p_element_name, unsigned int &p_index)
    {
        unsigned int element_int_value = -1;
        /* First extract the header element */
        std::string element_value = _get_header_element(p_layer_content, p_index);
        /* Try to match the elementValue string */
        if (element_value.find(p_element_name) != std::string::npos)
        {
            size_t pos = element_value.find(p_element_name);
            /* If found then erase it from string */
            element_value.erase(pos, p_element_name.length());
            /* Dump it into the output variable using stoi */
            element_int_value = std::stoi(element_value);
        }
        else
        {
            printf("(Parsing Document) WARNING: Missing header element in tile with name '%s'\n", p_element_name.c_str());
        }
        return element_int_value;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    // Extract a header element starting from the current_index until the next "0x0A".
    // ---------------------------------------------------------------------------------------------------------------------
    std::string KraLayer::_get_header_element(const std::vector<unsigned char> &p_layer_content, unsigned int &p_index)
    {
        unsigned int begin_index = p_index;
        /* Just go through the vector until you encounter "0x0A" (= the hex value of Line Feed) */
        while (p_layer_content.at(p_index) != (char)0x0A)
        {
            p_index++;
        }
        unsigned int end_index = p_index;
        /* Extract this header element */
        std::string element_value(p_layer_content.begin() + begin_index, p_layer_content.begin() + end_index);
        /* Increment the current_index pointer so that we skip the "0x0A" character */
        p_index++;

        return element_value;
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
};