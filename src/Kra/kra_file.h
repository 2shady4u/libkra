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

#include <tinyxml2/tinyxml2.h>
#include <zlib/contrib/minizip/unzip.h>
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
	unsigned int ParseUIntAttribute(const tinyxml2::XMLElement *xmlElement, const char *attributeName);
	const char *ParseCharAttribute(const tinyxml2::XMLElement *xmlElement, const char *attributeName);
	const wchar_t *ParseWCharAttribute(const tinyxml2::XMLElement *xmlElement, const char *attributeName);

	std::vector<std::unique_ptr<KraLayer>> ParseLayers(tinyxml2::XMLElement *xmlElement);
	std::vector<std::unique_ptr<KraTile>> ParseTiles(std::vector<unsigned char> layerContent);

	unsigned int ParseHeaderElement(std::vector<unsigned char> layerContent, const std::string &elementName, unsigned int &currentIndex);
	std::string GetHeaderElement(std::vector<unsigned char> layerContent, unsigned int &currentIndex);

	int extractCurrentFileToVector(std::vector<unsigned char> &resultVector, unzFile &m_zf);
	int lzff_decompress(const void *input, int length, void *output, int maxout);

public:
	unsigned int width;
	unsigned int height;
	unsigned int channelCount;

	const char *name;

	std::vector<std::unique_ptr<KraLayer>> layers;

	bool corruptionFlag = false;

	void load(const std::wstring &p_path);
	std::vector<std::unique_ptr<KraExportedLayer>> CreateKraExportLayers();
};

#endif // KRA_FILE_H