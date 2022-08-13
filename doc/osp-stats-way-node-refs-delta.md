
# NAME

osp-stats-way-node-refs-delta - Calculate stats for delta encoding of way nodes

# SYNOPSIS

**osp-stats-way-node-refs-delta** *OSM-FILE*

# DESCRIPTION

Ways contain an ordered list of node references. Often the difference between
one node id and the next is small, because node ids are given out sequentially
on creation of those nodes and nodes inside a way are often created
sequentially. For this reason it makes sense to delta encode those references
in ways.

This program creates statistics that allow deducing the space requirement
for storing the references when delta encoding and variable length encoded
integers (varint) are used. The statistics are printed to stdout.

# OPTIONS

# DIAGNOSTICS

**osp-stats-way-node-refs-delta** exits with exit code

0
  ~ if everything went alright,

1
  ~ if there was an error processing the data, or

2
  ~ if there was a problem with the command line arguments.

# MEMORY USAGE

No significant memory usage except for input buffers.

# EXAMPLES

# SEE ALSO

