
# NAME

osp-history-stats-basic - Calculate some basic statistics on OSM history files

# SYNOPSIS

**osp-history-stats-basic** \[*OPTIONS*\] *OSM-HISTORY-FILE*

# DESCRIPTION

Calculate some basic statistics from OSM history files for each day and write
those statistics to an SQLite database.

# OPTIONS

-h, \--help
:   Show usage help.

-o, \--output=FILE
:   Name of the output file.

-q, \--quiet
:   Quiet mode.

# DIAGNOSTICS

**osp-history-stats-basic** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

# EXAMPLES

# SEE ALSO

* **osp-stats-basic**

