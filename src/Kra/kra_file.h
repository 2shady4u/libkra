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

#include <stddef.h>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iostream>

#include <locale>
#include <codecvt>
#include <memory>

#define WRITEBUFFERSIZE (8192)

// KraTile is a structure in which the general properties of a KRA document/archive are stored.
// Each KRA archive consists of one or more layers (stored in a vector) that contain actual data.
class KraFile
{
private:
	std::vector<std::unique_ptr<KraLayer>> _parse_layers(unzFile p_file, tinyxml2::XMLElement *xmlElement);

public:
	enum VerbosityLevel
	{
		QUIET,
		VERBOSE
	};

	std::string name;

	unsigned int channel_count;
	unsigned int width;
	unsigned int height;

	std::vector<std::unique_ptr<KraLayer>> layers;

	bool corruption_flag = false;

	static VerbosityLevel verbosity_level;

	static int extract_current_file_to_vector(std::vector<unsigned char> &resultVector, unzFile &m_zf);

	void load(const std::wstring &p_path);
	std::unique_ptr<KraExportedLayer> get_exported_layer(int p_layer_index);
	std::vector<std::unique_ptr<KraExportedLayer>> get_all_exported_layers();
};

#endif // KRA_FILE_H