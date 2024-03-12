// ############################################################################ #
// Copyright © 2022-2024 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
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

#include <unordered_map>
#include <codecvt>
#include <locale>

namespace kra
{
	/* This class stores the general properties of a KRA/KRZ-archive as well as a vector of layers containing the actual data */
	class Document
	{
	private:
		std::vector<std::unique_ptr<Layer>> _parse_layers(unzFile &p_file, const tinyxml2::XMLElement *xmlElement);

		void _create_layer_map();
		void _add_layer_to_map(const std::unique_ptr<Layer> &layer);

	public:
		std::string name;

		unsigned int width;
		unsigned int height;

		ColorSpace color_space;

		std::vector<std::unique_ptr<Layer>> layers;

		std::unordered_map<std::string, const std::unique_ptr<Layer> &> layer_map;

		void load(const std::wstring &p_path);

		std::unique_ptr<ExportedLayer> get_exported_layer_at(int p_layer_index) const;
		std::unique_ptr<ExportedLayer> get_exported_layer_with_uuid(const std::string &p_uuid) const;

		std::vector<std::unique_ptr<ExportedLayer>> get_all_exported_layers() const;

		void print_document_attributes() const;
	};
};

#endif // KRA_DOCUMENT_H