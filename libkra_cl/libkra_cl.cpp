// ############################################################################ #
// Copyright © 2022 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "../libkra/kra_utility.h"

#include "../libkra/kra_document.h"
#include "../libkra/kra_exported_layer.h"

#include "../libpng/png.h"

#include <iostream>

// ---------------------------------------------------------------------------------------------------------------------
// Export and save as a *.png-file with the help of the libpng-library.
// ---------------------------------------------------------------------------------------------------------------------
bool write_data_to_png(const char *filename, unsigned int width, unsigned int height, const uint8_t *data)
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
// Get the important layer data and write this data to a .png-file
// ---------------------------------------------------------------------------------------------------------------------
void save_layer_to_image(const std::unique_ptr<kra::ExportedLayer> &layer)
{
	unsigned int layer_width = (unsigned int)(layer->right - layer->left);
	unsigned int layer_height = (unsigned int)(layer->bottom - layer->top);
	const std::string file_name = layer->name + ".png";

	/* Export the layer's data to a texture */
	write_data_to_png(file_name.c_str(), layer_width, layer_height, layer->data.data());
}

// ---------------------------------------------------------------------------------------------------------------------
// Process each layer and, depending on the type, either call the saving method or recursively call this method again.
// ---------------------------------------------------------------------------------------------------------------------
void process_layer(const std::unique_ptr<kra::Document> &document, const std::unique_ptr<kra::ExportedLayer> &layer)
{
	switch (layer->type)
	{
	case kra::PAINT_LAYER:
	{
		save_layer_to_image(layer);
		break;
	}
	case kra::GROUP_LAYER:
		for (auto const &uuid : layer->child_uuids)
		{
			std::unique_ptr<kra::ExportedLayer> child = document->get_exported_layer_with_uuid(uuid);

			process_layer(document, child);
		}
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Export the document as found at the given path
// ---------------------------------------------------------------------------------------------------------------------
int export_document(std::wstring p_file_name)
{
	std::unique_ptr<kra::Document> document = std::make_unique<kra::Document>();
	document->load(p_file_name);
	if (document == NULL)
	{
		return 1;
	}

	switch (document->color_space)
	{
	case kra::ColorSpace::RGBA:
	{
		std::vector<std::unique_ptr<kra::ExportedLayer>> exported_layers = document->get_all_exported_layers();
		for (auto const &layer : exported_layers)
		{
			process_layer(document, layer);
		}
		return 0;
	}
	default:
		// NOTE: 16-bit integer images (RGBA16) can definitely be exported to PNG, but this is not implemented!
		std::fprintf(stderr, "ERROR: Document with color space name '%s' cannot be exported to PNG.\n", kra::get_color_space_name(document->color_space).c_str());
		return 1;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
static void show_usage(std::string name)
{
	// TODO: Allow multiple sources!
	// TODO: Add a destination option at some point!
	std::cerr << "Usage: " << name << " [options]\n"
			  << "\n"
			  << "General options:\n"
			  << "  -h, --help                       Display this help message.\n"
			  << "  -s, --source <source>            Specify the KRA source file.\n"
			  << "  -q, --quiet                      Do not print anything in the console.\n"
			  << "  -v, --verbose                    Print additional logs in the console.\n";
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
int main(int argc, const char *argv[])
{
	std::vector<std::string> sources;
	// NOTE: Maybe we shouldn't hardcode this? This is here mainly for debugging purposes.
	std::wstring file_name = L"..\\examples\\example_RGBA.kra";

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
				file_name = std::wstring(str.begin(), str.end());
			}
			else
			{ // Uh-oh, there was no argument to the source option.
				std::cerr << "--source option requires one argument." << std::endl;
				return 1;
			}
		}
		else if ((arg == "-q") || (arg == "--quiet"))
		{
			kra::verbosity_level = kra::QUIET;
		}
		else if ((arg == "-v") || (arg == "--verbose"))
		{
			kra::verbosity_level = kra::VERBOSE;
		}
		else
		{
			sources.push_back(argv[i]);
		}
	}

	const int result = export_document(file_name);
	if (result != 0)
	{
		return result;
	}

	return 0;
}