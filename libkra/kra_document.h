// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#ifndef KRA_DOCUMENT_H
#define KRA_DOCUMENT_H

#include "kra_utility.h"

#include "kra_layer.h"
#include "kra_exported_layer.h"

#include "../tinyxml2/tinyxml2.h"
#include "../zlib/contrib/minizip/unzip.h"

#include <stddef.h>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iostream>

#include <locale>
#include <codecvt>
#include <memory>
#include <unordered_map>

namespace kra
{
	// KraTile is a structure in which the general properties of a KRA document/archive are stored.
	// Each KRA archive consists of one or more layers (stored in a vector) that contain actual data.
	class KraDocument
	{
	private:
		std::vector<std::unique_ptr<KraLayer>> _parse_layers(unzFile p_file, tinyxml2::XMLElement *xmlElement);

		void _create_layer_map();
		void _add_layer_to_map(const std::unique_ptr<KraLayer> &layer);

	public:
		std::string name;

		unsigned int channel_count;
		unsigned int width;
		unsigned int height;

		std::vector<std::unique_ptr<KraLayer>> layers;

		bool corruption_flag = false;

		std::unordered_map<std::string, const std::unique_ptr<KraLayer> &> layer_map;

		void load(const std::wstring &p_path);

		std::unique_ptr<KraExportedLayer> get_exported_layer_at(int p_layer_index) const;
		std::unique_ptr<KraExportedLayer> get_exported_layer_with_uuid(const std::string &p_uuid) const;

		std::vector<std::unique_ptr<KraExportedLayer>> get_all_exported_layers() const;
	};
};

#endif // KRA_DOCUMENT_H