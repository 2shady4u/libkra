// ############################################################################ #
// Copyright © 2022-2026 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_KEYFRAME_H
#define KRA_KEYFRAME_H

#include "kra_utility.h"

#include "kra_layer_data.h"

#include "../tinyxml2/tinyxml2.h"

namespace kra
{
    /* This class stores the attributes (as found in 'layer*.keyframes.xml') for a single keyframe */
    class Keyframe
    {
    private:

    public:
        std::string frame;
        unsigned int time;

        std::unique_ptr<LayerData> layer_data;

        void import_attributes(const std::string &p_name, unzFile &p_file, const tinyxml2::XMLElement *p_xml_element);
    };
};

#endif // KRA_KEYFRAME_H