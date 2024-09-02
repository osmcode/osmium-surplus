
# NAME

osp-history-stats-users-coedit - Create co-edit graph from OSM history file

# SYNOPSIS

**osp-history-stats-users** \[*OPTIONS*\] *OSM-HISTORY-FILE*

# DESCRIPTION

Count how many objects were edited by a pair of users and create a graph from
this data. It is written as osmcoedit.dot to the output directory.

# OPTIONS

-h, \--help
:   Show usage help.

-o, \--output=FILE
:   Name of the output directory.

-q, \--quiet
:   Quiet mode.

# DIAGNOSTICS

**osp-history-stats-users-coedit** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

About 5 GB on a history planet.

# EXAMPLES

# SEE ALSO

