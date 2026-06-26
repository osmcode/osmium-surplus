
# NAME

osp-stats-basic - Calculate some basic statistics on OSM data files

# SYNOPSIS

**osp-stats-basic** \[*OPTIONS*\] *OSM-FILE*

# DESCRIPTION

Calculate some basic statistics from OSM data file and write them to an SQLite
database. If the input is an OSM history file, some of the stats will not make
sense or count something different than what you might expect, because the file
is read as if it was a normal OSM data file.

If the input is an OSM history file, you can use the *\--timestamp* option to
change the processing mode: In this case processing is done as if you had
created a point-in-time extract of the history file for that timestamp.

# OPTIONS

-h, \--help
:   Show usage help.

-o, \--output=FILE
:   Name of the output file.

-q, \--quiet
:   Quiet mode.

-t, \--timestamp
:   If the command is run on an OSM history file this option can be used to
    specify the point in time for which the stats should be calculated.

# DIAGNOSTICS

**osp-stats-basic** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

# EXAMPLES

# SEE ALSO

* **osp-history-stats-basic**

