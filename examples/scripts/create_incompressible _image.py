# This script creates an image with data that breaks LZF compression.
# Importing this image as a layer in Krita (and saving it) results in an uncompressed tile.
# The resulting archive 'example_uncompressed.kra' is found in the parent folder.

import os
from PIL import Image

width: int = 64
height: int = 64
total_bytes: int = width * height * 4  # 64x64 RGBA = 16,384 bytes

# Generate true cryptographic random bytes
raw_bytes = os.urandom(total_bytes)

img: Image = Image.frombytes("RGBA", (width, height), raw_bytes)

# Save as PNG without compression overhead (level 0 preserves exact raw values)
output_file: str = "max_entropy_64x64.png"
img.save(output_file, compress_level=0)

print(f"File created: {output_file}")
