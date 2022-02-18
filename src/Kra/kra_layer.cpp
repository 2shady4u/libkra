#include "kra_layer.h"

void KraLayer::import_attributes(const tinyxml2::XMLElement *p_xml_element)
{
    /* Get important layer attributes from the XML-file */
    filename = p_xml_element->Attribute("filename");
    name = p_xml_element->Attribute("name");
    uuid = p_xml_element->Attribute("uuid");

    x = p_xml_element->UnsignedAttribute("x", 0);
    y = p_xml_element->UnsignedAttribute("y", 0);
    opacity = p_xml_element->UnsignedAttribute("opacity", 0);

    visible = p_xml_element->BoolAttribute("visible", true);
}