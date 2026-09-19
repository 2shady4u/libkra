// ############################################################################ #
// Copyright © 2022-2026 Piet Bronders & Jeroen De Geeter <piet.bronders@gmail.com>
// Licensed under the MIT License.
// See LICENSE in the project root for license information.
// ############################################################################ #

#include "../libkra/kra_utility.h"

#include "../libkra/kra_document.h"
#include "../libkra/kra_exported_layer.h"

#include "../libpng/png.h"

#include <iostream>
#include <fstream>

// Custom write callback for libpng to be able to work with std::ofstream
static void user_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	std::ofstream* stream = static_cast<std::ofstream*>(png_get_io_ptr(png_ptr));
	stream->write(reinterpret_cast<char*>(data), length);
}

// Custom flush callback for libpng to be able to work with std::ofstream
static void user_flush_data(png_structp png_ptr)
{
	std::ofstream* stream = static_cast<std::ofstream*>(png_get_io_ptr(png_ptr));
	stream->flush();
}

// ---------------------------------------------------------------------------------------------------------------------
// Export and save as a *.png-file with the help of the libpng-library.
// ---------------------------------------------------------------------------------------------------------------------
bool write_data_to_png(const char *filename, unsigned int width, unsigned int height, const uint8_t *data)
{
	// Open file for writing (binary mode)
	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open())
	{
		std::cout << "Could not open file " << filename << " for writing" << std::endl;
		return false;
	}

	// Initialize write structure
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		std::cout << "Could not allocate write struct" << std::endl;
		return false;
	}

	// Initialize info structure
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		std::cout << "Could not allocate info struct" << std::endl;
		png_destroy_write_struct(&png_ptr, NULL);
		return false;
	}

	// RAII guard guarantees cleanup of png objects on any exit path
	struct PngCleanup {
		png_structp& ptr;
		png_infop& info;
		~PngCleanup() {
			if (ptr) {
				png_destroy_write_struct(&ptr, info ? &info : NULL);
			}
		}
	} png_guard{png_ptr, info_ptr};

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		std::cout << "Error during png creation" << std::endl;
		return false;
	}

	png_set_write_fn(png_ptr, &file, user_write_data, user_flush_data);

	const unsigned int channelCount = 4;
    const int colorType = PNG_COLOR_TYPE_RGBA;
	const int bitDepth = 8;

	/* Write header depending on the channel type, always in 8 bit colour depth. */
	png_set_IHDR(png_ptr, info_ptr, width, height,
				 bitDepth, colorType, PNG_INTERLACE_NONE,
				 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* Write image data, one row at a time. */
	size_t row_stride = static_cast<size_t>(width) * channelCount;
	for (unsigned int y = 0; y < height; y++)
	{
		png_const_bytep row_ptr = (png_bytep)(data + (y * row_stride));
		png_write_row(png_ptr, const_cast<png_bytep>(row_ptr));
	}

	/* End the png_ptrwrite operation */
	png_write_end(png_ptr, NULL);

	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Get the important layer data and write this data to a .png-file
// ---------------------------------------------------------------------------------------------------------------------
void save_layer_to_image(const std::unique_ptr<kra::ExportedLayer> &layer)
{
	unsigned int layer_width = (unsigned int)(layer->right - layer->left);
	unsigned int layer_height = (unsigned int)(layer->bottom - layer->top);
	const std::string file_name = layer->name + ".png";

	if (layer->data.empty()) {
		/* Ideally, we should fully populate the layer with the default pixel */
		/* For now, let's just avoid this situation */
		std::fprintf(stdout, "WARNING: Skipping empty layer with name '%s'.\n", layer->name.c_str());
		return;
	}


	/* Export the layer's data to a texture */
	write_data_to_png(file_name.c_str(), layer_width, layer_height, layer->data.data());
}

// ---------------------------------------------------------------------------------------------------------------------
// Write the raw SVG document of a vector layer to a file
// ---------------------------------------------------------------------------------------------------------------------
void save_layer_to_svg(const std::unique_ptr<kra::ExportedLayer> &layer)
{
	const std::string file_name = layer->name + ".svg";

	std::ofstream file(file_name, std::ios::binary);
	if (!file.is_open())
    {
		std::fprintf(stderr, "ERROR: Could not open '%s' for writing.\n", file_name.c_str());
		return;
	}

	file.write(reinterpret_cast<const char*>(layer->svg_content.data()), layer->svg_content.size());
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
	case kra::VECTOR_LAYER:
		save_layer_to_svg(layer);
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Export the document as found at the given path
// ---------------------------------------------------------------------------------------------------------------------
int export_document(std::wstring p_file_name)
{
	std::unique_ptr<kra::Document> document = std::make_unique<kra::Document>();
	const int result = document->load(p_file_name);
	if (result != 0)
	{
		return result;
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
	std::wstring file_name = L"../examples/example_RGBA.kra";

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
