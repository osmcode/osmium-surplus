
# NAME

osp-filter-relations-types - Split input file based on relation types

# SYNOPSIS

**osp-filter-relations-types** \[*OPTIONS*\] *OSM-FILE*

# DESCRIPTION

Reads OSM data file and writes out an OSM file for each relation type
containing all relations of that type as well as their members (recursively).
The OSM output files are named after the type with suffix `.osm.pbf`. Because
the type could contain unprintable characters, all characters other than
a-z, A-Z, _ (underscore), - (hyphen), and : (semicolon) are printed as `@`.
Relation without `type` tag are stored in the file `UNKNOWN.osm.pbf`.

In addition a SQLite database `relation-types.db` is created in the output
directory containing a single table caled `types` which contains a list
of types, the file basenames used and a count of relations of that type.

# BUGS

Doesn't yet handle relation members of relations correctly.

# OPTIONS

-h, \--help
:   Show usage help.

-o, \--output=DIR
:   Name of the output directory.

-q, \--quiet
:   Quiet mode.

# DIAGNOSTICS

**osp-filter-relations-types** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

The program needs to store which objects to include in which files. This needs
8 bytes per object.

# EXAMPLES

# SEE ALSO

* **osp-filter-relations-and-members**(1)

