# libkra

A C++ library for importing Krita's KRA & KRZ formatted documents.

### Supported operating systems:
- Mac OS X
- Linux
- Windows

### Table Of Contents

- [External Dependencies](#bill-of-dependencies)
- [Known Limitations](#known-limitations)

# <a name="external-dependencies">External Dependencies</a>

### Essential dependencies:

- [zlib](https://www.zlib.net/)

    **Purpose:** Unzipping the `.kr(a/z)`-archive

- [TinyXML-2](http://www.grinninglizard.com/tinyxml2/index.html)

    **Purpose:** Parsing the `maindoc.xml`-file as found in the unzipped archive

### Optional Dependencies:

- [libpng](http://www.libpng.org/pub/png/libpng.html)

    **Purpose:** Saving the exported layer data to `.png`-files for demonstration purposes

# <a name="known-limitations">Known Limitations</a>

...