# Osmium Surplus

This is a collection of assorted small programs based on the [Osmium
library](https://osmcode.org/libosmium). I work a lot with OSM data and over
the years needed a lot of different programs to create statistics, filter or
analyze OSM data, or process it in some other way. This is where all those
programs end up that are not "large" enough to warrant their own repository.

This repository also contains some programs moved over from earlier separate
repositories.

## Status

The quality of these programs varies a lot. Some are reasonably polished, some
are not much more than experiments. I don't promise any level of fitness for
any purpose, but please do open an issue if you have a problem with any of
these.

## Preqrequisites

You need a C++17 compliant compiler. You also need the following libraries:

    Libosmium (>= 2.17.0)
        https://osmcode.org/libosmium
        Debian/Ubuntu: libosmium2-dev
        Fedora/CentOS: libosmium-devel

    Protozero (>= 1.6.3)
        https://github.com/mapbox/protozero
        Debian/Ubuntu: libprotozero-dev
        Fedora/CentOS: protozero-devel

    bz2lib
        http://www.bzip.org/
        Debian/Ubuntu: libbz2-dev
        Fedora/CentOS: bzip2-devel
        openSUSE: libbz2-devel

    zlib
        https://www.zlib.net/
        Debian/Ubuntu: zlib1g-dev
        Fedora/CentOS: zlib-devel
        openSUSE: zlib-devel

    Expat
        https://libexpat.github.io/
        Debian/Ubuntu: libexpat1-dev
        Fedora/CentOS: expat-devel
        openSUSE: libexpat-devel

    fmt
        https://fmt.dev/
        Debian/Ubuntu: libfmt-dev

    GDAL/OGR
        https://gdal.org/
        Debian/Ubuntu: libgdal-dev

    Sqlite
        https://sqlite.org/
        Debian/Ubuntu: libsqlite3-dev
        Fedora/CentOS: sqlite-devel

    cmake
        https://cmake.org/
        Debian/Ubuntu: cmake
        Fedora/CentOS: cmake
        openSUSE: cmake

Not all programs have all dependenices, and some programs might have additional
dependencies.

Some program use the [CLI11](https://github.com/CLIUtils/CLI11) library for
parsing the command line. A version of this is included in the `include`
directory.

Some programs use the [Lyra library](https://github.com/bfgroup/Lyra) for
parsing the command line options. It is included in the `include` directory.

## Building

These programs uses CMake for their builds. On Linux and macOS you can build as
follows:

    cd osmium-surplus
    mkdir build
    cd build
    cmake ..
    ccmake .  ## optional: change CMake settings if needed
    make

To set the build type call cmake with `-DCMAKE_BUILD_TYPE=type`. Possible
values are empty, Debug, Release, RelWithDebInfo, MinSizeRel. The
default is RelWithDebInfo.

Please read the CMake documentation and get familiar with the `cmake` and
`ccmake` tools which have many more options.

## Documentation

See the [doc](doc/) directory for a list of programs and their documentation.

Some additional information for the following commands detecting problems and
anomalies in the OSM data:

* osp-find-colocated-nodes
* osp-find-orphans
* osp-find-unusual-tags
* osp-find-way-problems
* osp-find-relation-problems
* osp-find-multipolygon-problems

These programs create

* one or more OSM PBF files with the data of the different anomalies they
  detected,
* an Sqlite file called `stats-*.db` with statistical data, and
* a Spatialite file called `geoms-*.db` containing geometries of the
  data detected (only for some commands).

You can use the script `scripts/collect-stats.sh` to collect the stats from
the various commands into one database called `stats.db`. All stats contain
a timestamp, so you can aggregate stats from, say, daily runs into one large
database.

The timestamp on the stats is the last timestamp of any object in the input
file. This may differ slightly between the various commands, because not all
commands read all object types.

## Contributing

Contributions are welcome. Please use `clang-format` to format your changes.

## Author

Jochen Topf (jochen@topf.org)

