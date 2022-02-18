// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_FILE_H
#define KRA_FILE_H

#include "kra_layer.h"
#include "kra_exported_layer.h"

#include "../tinyxml2/tinyxml2.h"
#include "../../zlib/contrib/minizip/unzip.h"

#include <regex>
#include <stddef.h>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>

#include <locale>
#include <codecvt>
#include <memory>

#define WRITEBUFFERSIZE (8192)

// KraTile is a structure in which the general properties of a KRA document/archive are stored.
// Each KRA archive consists of one or more layers (stored in a vector) that contain actual data.
class KraFile
{
private:
	std::vector<std::unique_ptr<KraLayer>> _parse_layers(tinyxml2::XMLElement *xmlElement);
	std::vector<std::unique_ptr<KraTile>> _parse_tiles(std::vector<unsigned char> layerContent);

	unsigned int _parse_header_element(std::vector<unsigned char> layerContent, const std::string &elementName, unsigned int &currentIndex);
	std::string _get_header_element(std::vector<unsigned char> layerContent, unsigned int &currentIndex);

	int _extract_current_file_to_vector(std::vector<unsigned char> &resultVector, unzFile &m_zf);
	int _lzff_decompress(const void *input, int length, void *output, int maxout);

public:
	unsigned int width;
	unsigned int height;
	unsigned int channel_count;

	const char *name;

	std::vector<std::unique_ptr<KraLayer>> layers;

	bool corruptionFlag = false;

	void load(const std::wstring &p_path);
	std::unique_ptr<KraExportedLayer> get_exported_layer(int p_layer_index);
	std::vector<std::unique_ptr<KraExportedLayer>> get_all_exported_layers();
};

#endif // KRA_FILE_H