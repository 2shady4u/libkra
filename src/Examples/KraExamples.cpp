// ############################################################################ #
// Copyright © 2020 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Copyright © 2020 Gamechuck d.o.o. <gamechuckdev@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "../Kra/kra_file.h"
#include "../Kra/kra_exported_layer.h"

#include "../../libpng/png.h"

// ---------------------------------------------------------------------------------------------------------------------
// Here the data gets exported and saved as a png using the libpng library.
// ---------------------------------------------------------------------------------------------------------------------
bool writeImage(const char *filename, unsigned int width, unsigned int height, const uint8_t *data)
{

	bool success = true;
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytep row = NULL;

	unsigned int channelCount = 4;
	int colorType = PNG_COLOR_TYPE_RGBA;

	// Open file for writing (binary mode)
	fp = fopen(filename, "wb");
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
int ExampleReadKra(std::wstring rawFile)
{
	std::unique_ptr<KraFile> document = std::make_unique<KraFile>();
	document->load(rawFile);
	if (document == NULL)
	{
		return 1;
	}

	std::vector<std::unique_ptr<KraExportedLayer>> exportedLayers = document->get_all_exported_layers();

	for (auto const &layer : exportedLayers)
	{
		unsigned int layerHeight = (unsigned int)(layer->bottom - layer->top);
		unsigned int layerWidth = (unsigned int)(layer->right - layer->left);
		// std::unique_ptr<uint8_t[]> data = std::move(layer->data);
		/* Export the layer's data to a texture */
		std::stringstream ssFilename;
		ssFilename << layer->name;
		ssFilename << ".png";
		/* TODO: Add the actual exporting functionality here! */
		writeImage(ssFilename.str().c_str(), layerWidth, layerHeight, layer->data.get());
	}

	return 0;
}

static void show_usage(std::string name)
{
	std::cerr << "Usage: " << name << " [options]\n"
			  << "\n"
			  << "General options:\n"
			  << "  -h, --help                       Display this help message.\n"
			  << "  -s, --source <source>            Specify the KRA source file.\n"
			  << "  -q, --quiet                      Do not print anything in the console.\n";
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
	std::vector<std::string> sources;
	std::wstring rawFile = L"..\\..\\bin\\KRAExample.kra";
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if ((arg == "-h") || (arg == "--help"))
		{
			show_usage(argv[0]);
			return 0;
		}
		else if ((arg == "-s") || (arg == "--source"))
		{
			// Make sure we aren't at the end of argv!
			if (i + 1 < argc)
			{
				std::string str = argv[i + 1]; // Increment 'i' so we don't get the argument as the next argv[i].
				rawFile = std::wstring(str.begin(), str.end());
			}
			else
			{ // Uh-oh, there was no argument to the source option.
				std::cerr << "--source option requires one argument." << std::endl;
				return 1;
			}
		}
		else if ((arg == "-q") || (arg == "--quiet"))
		{
			KraFile::verbosity_level = KraFile::QUIET;
		}
		else
		{
			sources.push_back(argv[i]);
		}
	}

	{
		const int result = ExampleReadKra(rawFile);
		if (result != 0)
		{
			return result;
		}
	}

	return 0;
}