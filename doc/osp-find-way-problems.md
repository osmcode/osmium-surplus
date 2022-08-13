
# NAME

osp-find-way-problems - Find ways with problems

# SYNOPSIS

**osp-find-way-problems** \[*OPTIONS*\] *OSM-FILE* *OUTPUT-DIR*

# DESCRIPTION

Finds several problems with way geometries:

* Way has no nodes (`no-node`).
* Way has only a single node (`single-node`).
* Way has more than one node, but all nodes are the same (`same-node`).
* Way has two or more references to the same node one right after the other
  (`duplicate-node`).
* Way intersects itself at a place where there is no node (`self-intersection`).
* Way contains a "spike", a segment from node A to node B and, directly after
  that a segment back from node B to node A (`spike`).
* Way contains a duplicate segment, so a connection between two nodes is
  in the way more than once (regardless of the direction of that segment)
  (`duplicate-segment`).

This command needs as input an OSM file with node locations on ways. See the
osmium
[add-locations-to-ways](https://docs.osmcode.org/osmium/latest/osmium-add-locations-to-ways.html)
command on how to create this.

# OPTIONS

-a, \--min-age=DAYS
:   Only include objects at least DAYS days old. Can not be used together with
    \--before.

-b, \--before=TIMESTAMP
:   Only include objects changed last before this time
    (format: `yyyy-mm-ddThh:mm:ssZ`). Can not be used together with \--min-age.

-h, \--help
:   Show usage help.

-m, \--max-nodes=NUM
:   Report ways with more nodes than this (default: 1800).

-q, \--quiet
:   Work quietly.

# DIAGNOSTICS

# MEMORY USAGE

# EXAMPLES

# SEE ALSO

