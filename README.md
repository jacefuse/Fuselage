# Fuselage

Free Unrestricted Software Enabling Layered Asset Game Environments
-------------------------------------------------------------------

The goal of Fuselage is to create an emulator and development framework designed to emulate a retro-inspired system that could have plausibly existed in the late 90s or early 2000s. It serves as a foundation for creating pixel-based games and applications, leveraging a combination of advanced graphical features and retro aesthetics.

- Key components in a working state...

    Text Layer (textlayer.h and textlayer.cpp):
        Implements an 80x45 text grid with 16x16-pixel cells for character rendering. Features text rendering, cursor control, scrolling, and dynamic text coloring. Uses a character atlas for efficient texture management. Includes flexible tlPrint functions, supporting variadic arguments and overloads for easy text handling.

    Color Palette (colors.h and colors.c):
        Supports 256 palettes, each containing 16 colors, rather than a flat palette of 256 colors. Provides utilities to manage palettes, including functions to set individual colors, initialize palettes, and load sprite-based palettes. Predefined color sets include grayscale, primary colors, and retro-styled shades.

    Sprite Layer (sprites.h and sprites.c):
        Manages up to 640 sprites, with properties like position, scale, rotation, transparency, and assigned color palette. Implements collision detection (both bounding box and pixel-level) and supports layering for sprite rendering. Leverages a sprite atlas for efficient texture handling and reduced memory usage.

    Sprite conversion tool (spriteconverter.c):
        Handles conversion of image data into a compatible format for Fuselage sprites. Ensures palette compatibility by validating and generating palettes that conform to the 16-color-per-palette limit. Generates C-style header files and palette visualization files.
- Current Work In Progress...

    Tile Layer (tiles.h and tiles.c):
        A multilayered tile handling system that will allow for embedded assets to be used as tilesets and tilemaps.

- Planned and Future Goals...

    System Architecture:
        Transition to a psuedo RISC-V-based emulated architecture (a "VPU" if you will), allowing for a realistic and extensible CPU model. Develop a virtual machine capable of simulating assembly-like programming for games and system-level logic. Use of the VPU would be optional, as every component of Fuselage will be usable directly on the target platforms through C and C++. Ideally, ports of the framework to other systems will be as close to system-native and will rely on as few external dependancies as possible.

    Pixie Layer:
        Introduce the Pixie Layer (short for "PIXel Information Encoding") to handle sophisticated bitmap operations. The Pixie Layer will serve as a flexible system for managing and rendering complex pixel-based data, extending the capabilities of both the Text and Sprite Layers. Pixie data is not just RGBA values, but instead a collection of graphic and operation data allowing graphics objects to act not as static images but rather as image output routines. 

    Rendering Framework:
        Move away from RayLib and adopt Vulkan for rendering. This shift will provide low-level access to GPU hardware for greater performance and flexibility. RayLib is a wonderful tool for prototyping and getting the project off of the ground but the performance and efficiency goals of Fuselage will benefit from a focus on Vulkan. This will happen sooner than later.

    Tooling and Workflow:
        Since the goal of Fuselage is to make most applications developed with it use a static memory footprint and embedded assets, there will be a range of tools developed for the purpose of converting assets into source suitable for use with Fuselage. Of course none of this will be worth anything to anyone if nobody knows how to use it so ultimately the goal is to have all of the tools pretty self explained or well documented.

- Summary...
  
Fuselage aims to bring to life a "what if?" vision of a system that could have existed in the past, blending retro limitations with modern development practices. By leveraging RISC-V for the virtual machine, Vulkan for rendering, feature rich sprite, tile, and Pixie Layers for 2D graphics processing, Fuselage seeks to create an extensible platform for retro-inspired creativity while maintaining a strong focus on modularity, efficiency, and performance.
