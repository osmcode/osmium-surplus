
# NAME

osp-find-and-fix-control-characters - Find and fix control characters in tags

# SYNOPSIS

**osm-find-and-fix-control-characters** \[*OPTIONS*\] *OSM-INPUT-FILE*

# DESCRIPTION

Control characters are not allowed in OSM data, because they are not allowed in
XML files. But they do sometimes end up in the OSM database.

This program finds all instances of control characters in an OSM file and,
optionally, removes them.

Note that it doesn't make sense to run this program on XML files with invalid
characters, because the XML parser will complain and the data will never get
through to libosmium.

The *OSM-INPUT-FILE* is read and all objects containing invalid characters are
written the the *ERROR-FILE*. If an *OUTPUT-FILE* was given, all data will be
written to that file with invalid characters removed.

The program will also count the number of errors and print them to STDERR.

# OPTIONS

-h, \--help
:   Show usage help.

-e, \--error-file=ERROR-FILE
:   Name of the error file (Required).

-o, \--output=OUTPUT-FILE
:   Name of the output file.

-q, \--quiet
:   Quiet mode.

# DIAGNOSTICS

**osp-find-and-fix-control-characters** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

# EXAMPLES

# SEE ALSO

