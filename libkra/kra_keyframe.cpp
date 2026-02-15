// ############################################################################ #
// Copyright © 2022-2026 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "kra_keyframe.h"

namespace kra
{
    // ---------------------------------------------------------------------------------------------------------------------
    // ???
    // ---------------------------------------------------------------------------------------------------------------------
    void Keyframe::import_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element)
    {
        frame = p_xml_element->Attribute("frame");
        time = p_xml_element->UnsignedAttribute("time");

        const std::string &layer_path = p_name + "/layers/" + frame;
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
    }
};