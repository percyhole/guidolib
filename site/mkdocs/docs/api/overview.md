# Overview of the Guido library

The Guido Engine operates on a memory representation of the [Guido Music Notation](http://guidodoc.grame.fr) format [GMN]
the Guido Abstract Representation [AR]. This representation is transformed step by step 
to produce graphical score pages. Two kinds of processing are applied to the AR:

- AR to AR transformations which represents a logical layout transformation: part of the
layout (such as beaming for example) may be computed from the AR as well as expressed in AR,
- the AR is finally converted into a Guido Graphic Representation (GR) that contains the
necessary layout information and is directly used to draw the music score. This final step 
includes notably spacing and page breaking algorithms.

See the Guido Project [overview](/overview/index.html) pages for a quick introduction to the library services. It gives also an overview of the score operations provided by the [GuidAR library](https://github.com/grame-cncm/guidoar).

See also the [system architecture](/internals/architecture).

## Main services

The main services provided by the library are:

- [Parsing](/dox/api/group__Parser.html) GMN files, strings or streams to build an AR memory representation. GMN streams are _unfinished_ GMN representations, intended to be written on the fly, that can be handled like regular GMN files,
- [Building AR](/dox/api/group__Factory.html) memory representations from scratch using the Guido Factory API,
- [Building GR representations](/dox/api/group__Engine.html) from AR representations and controlling the graphic representation,
- [Score drawing and page formatting](/dox/api/group__Format.html),
- [Browsing and querying](/dox/api/group__Pages.html) music pages,
- [Time to graphic mappings](/dox/api/group__Mapping.html) that are relations between graphic segments (rectangles) and time segments (intervals),
- [MIDI conversion](/dox/api/group__midi.html) (provided that the engine has been compiled with MIDI support),
- Miscellaneous functions such as [version number](/dox/api/group__Misc.html), management or [timing](/dox/api/group__time.html) measurements,
- Alternate representations:
    - [Piano Roll](/dox/api/group__PianoRoll.html),
    - [Reduced proportional representation](/dox/api/group__Rproportional.html),


## Interfaces

The Guido library API was originally designed as a C API but it's available also for the following languages:

- **C++**: a [C++ interface](/dox/api/group__APICplusplus.html) has been designed over the C API.
- **Java**: the Guido engine is available as a native library for Java. Thus the implementation is object oriented but the C API is globally respected: only the Guido prefix of the function names is not present in the methods names. See the [java documentation](/javadoc).
- **Javascript**: the Guido engine is available as a Web Assembly [WASM] library for Javascript and published on [NPM](https://www.npmjs.com/package/@grame/guidolib). The implementation is based on the C++ interface. See the [javascript documentation](/jsdoc).
- **Python**: a Python interface has been designed but has not been maintained for years.

In addition, the Guido engine services are also available as [Web API](/webdoc).

