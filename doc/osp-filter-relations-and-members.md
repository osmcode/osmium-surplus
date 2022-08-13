
# NAME

osp-filter-relations-and-members - Get relations and their members from OSM file

# SYNOPSIS

**osp-filter-relations-and-members** \[*OPTIONS*\] *OSM-FILE*

# DESCRIPTION

Reads OSM data file and writes out an OSM file with all relations from the
input file plus all the relation members and, recursively, all their members.
So in the end you have a file which contains all objects from the input
file that are relations or directly or indirectly in some relation.

# OPTIONS

-h, \--help
:   Show usage help.

-o, \--output=FILE
:   Name of the output file.

-q, \--quiet
:   Quiet mode.

# DIAGNOSTICS

**osp-filter-relations-and-members** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

The program needs to store which members to include. For this it needs about
1 bit times the largest node, way, and relation ids, respectively. In non-quiet
mode it will print the memory used.

# EXAMPLES

# SEE ALSO

* **osp-filter-relations-types**(1)

