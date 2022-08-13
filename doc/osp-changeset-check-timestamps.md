
# NAME

osp-changeset-check-timestamps - Compare timestamps in OSM changeset and data files

# SYNOPSIS

**osp-changeset-check-timestamps** \[*OPTIONS*\] *OSM-DATA-INPUT*

# DESCRIPTION

Reads a changeset dump and a planet file and checks for objects created outside
the time window given in their changeset. Writes out two files, one containing
the data and one with the changesets that failed that check.

# OPTIONS

-h, \--help
:   Show usage help.

-o, \--output=OSM-FILE
:   Name of the error OSM output file.

-c, \--changeset=CHANGESET-FILE
:   Name of the changeset input file.

-e, \--changeset-error=CHANGESET-FILE
:   Name of the changeset error output file.

-q, \--quiet
:   Quiet mode.

# DIAGNOSTICS

**osp-changeset-check-timestamp** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

# EXAMPLES

# SEE ALSO

