
# NAME

osp-stats-tags-on-nodes - Calculate some stats on node tags

# SYNOPSIS

**osp-stats-tags-on-nodes** *OSM-FILE*

# DESCRIPTION

Most nodes don't have any tags or just some "unimportant" tags such as
*created_by* that are deprecated. This command has an internal list of such
tags and creates stats based on that list and prints them to stdout.

# OPTIONS

# DIAGNOSTICS

**osp-stats-tags-on-nodes** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

No significant memory use besides input buffers.

# EXAMPLES

# SEE ALSO

