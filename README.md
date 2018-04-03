# PBRTParser

A parser written to convert pbrt scenes to yocto scenes (obj).

## How to use
Once compiled using cmake, run
```
parse <file_to_parse> <output_obj>
```

## TODO

- Environment maps don't work.
- Normal flipping (must be implemented in yocto).
- Fix memory leaks using shared pointers.
- File paths: better handling.
- Checker texture produce different colors.
- Review materials
- Textures: detect if there are textures that are not used
- Testures: implement mix textures.
- Textures: implemement 
- Test illumination, implement some hack for distant light.
- Uber material has index property that will use to create a constant texture for "eta" (seen in code)
- Render big scenes

## Credits
This software is intended to work with and is built upon the yocto library, which is developed at the following repository: https://github.com/xelatihy/yocto-gl.