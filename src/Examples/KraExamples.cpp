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

#include "../../libpng/png.h"

KRA_USING_NAMESPACE;

// ---------------------------------------------------------------------------------------------------------------------
// Here the data gets exported and saved as a png using the libpng library.
// ---------------------------------------------------------------------------------------------------------------------
bool writeImage(const wchar_t *filename, unsigned int width, unsigned int height, const uint8_t *data)
{

	bool success = true;
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytep row = NULL;

	unsigned int channelCount = 4;
	int colorType = PNG_COLOR_TYPE_RGBA;

	std::wstring ws(filename);
	std::string str(ws.begin(), ws.end());
	const char *cFilename = str.c_str();

	// Open file for writing (binary mode)
	fp = fopen(cFilename, "wb");
	if (fp == NULL)
	{
		std::cout << "Could not open file " << filename << " for writing" << std::endl;
		success = false;
		goto finalise;
	}

	// Initialize write structure
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
	{
		std::cout << "Could not allocate write struct" << std::endl;
		success = false;
		goto finalise;
	}

	// Initialize info structure
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		std::cout << "Could not allocate info struct" << std::endl;
		success = false;
		goto finalise;
	}

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		std::cout << "Error during png creation" << std::endl;
		success = false;
		goto finalise;
	}

	png_init_io(png_ptr, fp);

	/* Write header depending on the channel type, always in 8 bit colour depth. */
	png_set_IHDR(png_ptr, info_ptr, width, height,
				 8, colorType, PNG_INTERLACE_NONE,
				 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* Allocate memory for one row */
	row = (png_bytep)malloc(channelCount * width * sizeof(png_byte));

	/* Write image data, one row at a time. */
	unsigned int x, y;
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			memcpy(row, &data[channelCount * width * y], channelCount * width * sizeof(png_byte));
		}
		png_write_row(png_ptr, row);
	}

	/* End the png_ptrwrite operation */
	png_write_end(png_ptr, NULL);

/* Clear up the memory from the heap */
finalise:
	if (fp != NULL)
		fclose(fp);
	if (info_ptr != NULL)
		png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	if (png_ptr != NULL)
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	if (row != NULL)
		std::free(row);

	return success;
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
int ExampleReadKra(void)
{
	const std::wstring rawFile = L"..\\..\\bin\\KRAExample.kra";

	std::unique_ptr<KraDocument> document = CreateKraDocument(rawFile);

	std::vector<std::unique_ptr<KraExportedLayer>> exportedLayers = CreateKraExportLayers(document);

	for (auto const &layer : exportedLayers)
	{
		const wchar_t *layerName = layer->name;
		unsigned int layerHeight = (unsigned int)(layer->bottom - layer->top);
		unsigned int layerWidth = (unsigned int)(layer->right - layer->left);
		//std::unique_ptr<uint8_t[]> data = std::move(layer->data);
		/* Export the layer's data to a texture */
		std::wstringstream ssFilename;
		ssFilename << layerName;
		ssFilename << L".png";
		/* TODO: Add the actual exporting functionality here! */
		writeImage(ssFilename.str().c_str(), layerWidth, layerHeight, layer->data.get());
	}

	return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
int main(int argc, const char *argv[])
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