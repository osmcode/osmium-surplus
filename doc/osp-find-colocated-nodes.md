
# NAME

osp-find-colocated-nodes - Find nodes having the exact same location

# SYNOPSIS

**osp-find-colocated-nodes** \[*OPTIONS*\] *OSM-FILE* *OUTPUT-DIR*

# DESCRIPTION

"Colocated nodes" are nodes that have the exact same location. In OSM that
is usually an error, but it doesn't have to be. There could be two nodes at
the same latitude and longitude but in different heights for instance.

This tool will find all colocated nodes and write them to the output.

Note that the program will create 256 temporary files named `locations_xx.dat`
in the output directory and later remove them. If the program is interrupted
those temporary files might be left around.

# OPTIONS

-a, \--min-age=DAYS
:   Only include objects at least DAYS days old. Can not be used together with
    \--before.

-b, \--before=TIMESTAMP
:   Only include objects changed last before this time
    (format: `yyyy-mm-ddThh:mm:ssZ`). Can not be used together with \--min-age.

-h, \--help
:   Show usage help.

-q, \--quiet
:   Work quietly.

# DIAGNOSTICS

# MEMORY USAGE

The program will need between 1 and 2 GByte RAM for caches.

# EXAMPLES

# SEE ALSO

