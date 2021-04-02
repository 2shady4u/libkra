// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "../Kra/KraDocument.h"
#include "../Kra/KraExportedLayer.h"
#include "../Kra/KraParseDocument.h"
#include "../Kra/KraExport.h"

KRA_USING_NAMESPACE;

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
int ExampleReadKra(void)
{
	const std::wstring rawFile = L"..\\..\\bin\\KRAExample.kra";

	std::unique_ptr<KraDocument> document = CreateKraDocument(rawFile);

	std::vector<std::unique_ptr<KraExportedLayer>> exportedLayers = CreateKraExportLayers(document);

	for (auto const& layer : exportedLayers)
	{
		const wchar_t* layerName = layer->name;
		unsigned int layerHeight = (unsigned int)(layer->bottom - layer->top);
		unsigned int layerWidth = (unsigned int)(layer->right - layer->left);
		//std::unique_ptr<uint8_t[]> data = std::move(layer->data);
        /* Export the layer's data to a texture */
		/* TODO: Add the actual exporting functionality here! */
	}

	return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
int main(int argc, const char * argv[])
{
	{
		const int result = ExampleReadKra();
		if (result != 0)
		{
			return result;
		}
	}

	return 0;
}