
# NAME

osp-find-unusual-tags - Find objects with unusual tags

# SYNOPSIS

**osp-find-unusual-tags** \[*OPTIONS*\] *OSM-FILE* *OUTPUT-DIR*

# DESCRIPTION

Find "unusual" tags such as empty, very short or long keys, the key "role",
or tag "type=multipolygon" on a node or way.

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

# EXAMPLES

# SEE ALSO

