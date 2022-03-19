# libkra

A C++ library for importing Krita's KRA & KRZ formatted documents.

### Supported operating systems:
- Mac OS X
- Linux
- Windows

### Table Of Contents

- [External Dependencies](#bill-of-dependencies)
- [Known Limitations](#known-limitations)
- [Build Instructions](#build-instructions)

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

### 1. Lack of color profile awareness

This library does not implement nor is aware of the presence of [color profiles](https://en.wikipedia.org/wiki/ICC_profile). As a result, the color profile of the original Krita document needs to be the exact same as the format supported by the intended target application. If this limitation is not respected the color of your image's pixels might be significantly different between applications.

One potential way to introduce conversion between color profiles is to add [Little CMS](https://www.littlecms.com/) as an additional external dependency in this library.

### 2. Missing & untested color spaces

While Krita supports a large array of color spaces, this library only implements a select few of them. 
Following color spaces are currently supported:

|           | Supported?              | Notes                  |
|-----------|:-----------------------:|:----------------------:|
| `RGBA`    | :heavy_check_mark:      | 8-bit integer          |
| `RGBA16`  | :heavy_check_mark:*     | 16-bit integer         |
| `RGBAF16` | :heavy_check_mark:*     | 16-bit floating-point  |
| `RGBAF32` | :heavy_check_mark:      | 32-bit floating-point  |
| `CMYK`    | :heavy_check_mark:*     | 8-bit integer          |

\* *Untested as of 19/03/2022*

Additional color spaces might be added in future updates if and when demand arises.

# <a name="build-instructions">Build Instructions</a>

Following instructions are applicable to the `libkra_cl` command line executable, as found in the aptly named `libka_cl/`-folder, which allows users to export each layer of a `*.kra(a/z)`-archive to a corresponding `*.png`-file by using the command line interface.

***NOTE**: In cases where this library is to be used as an external dependency in an independent project, the `libkra_cl`-folder can be fully ignored.*

Evidently, as a first pre-requisite step, this repository needs to be cloned and the submodules need to be initialized using the following Git commands:
```
git clone https://github.com/2shady4u/libkra.git
cd libkra
git submodule update --init --recursive
```

Additional **installation pre-requisites** include:
- Visual Studio Community 2019 (or whatever other C++ compiler you prefer)
- SCons (CMake is also possible, but is untested!)
- Git

Afterwards, following steps should be taken:

## 1. Copy-paste and rename '*pnglibconf.h.prebuilt*'

Since these build instructions do not use any CMake utilities the config files for `libpng` are not automatically generated. Fortunately, the `libpng`-respository includes a default configuration file that can be used instead.

Copy the file found at following path:

`<path-to-repository>/libkra/libpng/scripts/pnglibconf.h.prebuilt`

paste it inside of the main `libpng/`-folder and rename it as such:

`<path-to-repository>/libkra/libpng/pnglibconf.h`

## 2. Compile the `libkra_cl` command line executable

The SContruct file found in this repository's root should now be sufficient to build this project's C++ source code for all supported platforms, as such:

```
scons p=<platform> target=<target>
```

with valid platform values being either `windows`, `linux` or `osx` and valid target values being debug or release.

***NOTE**: On Windows, this command should be used inside of the 'x64 Native Tools Command Prompt for VS201X', otherwise SCons won't be able to find the necessary build utilities.*

And... that's all folks! 

---

For further specifics regarding the exact steps in the compilation process, please check out the `.github\workflows\*.yml`- and `SConstruct`-scripts as found in this repository.

If any issues/conflicts, not covered in these instructions, are encountered when compiling this library, please feel free to open an issue.