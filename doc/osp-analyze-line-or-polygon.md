
# NAME

osp-analyze-line-or-polygon

# SYNOPSIS

**osp-analyze-line-or-polygon** \[*OPTIONS*\] INPUT-FILE

# DESCRIPTION

This program looks at all the ways in an OSM file and tries to classify the
closed ways as linear objects or area objects depending on the tags. This is
surprisingly complicated and there are several corner cases.

The program reads several expression lists from the directory specified with
`--expression`/`-e/`.

* Tags that don't tell us anything about whether an object is a linestring
  or polygon (examples: `name`, `source`, ...). We call these "neutral" tags.
  (There are actually three filter files read: `neutral-tags`, `meta-tags`,
  and `import-tags`.)
* Tags that tell us that the object is a linestring (examples: most `highway`
  tags). We call these "linestring" tags. This does not take into account
  that an `area=yes` tag could turn the object into an area.
* Tags that tell us that the object is a polygon (examples: most `landuse`
  tags). We call these "polygon" tags. This does not take into account
  that an `area=no` tag could turn the object into an area.

We go through every way in the input file and, ignoring any "neutral" tags,
evaluate what tags we got. Usually we can decide whether something must be
a linestring or a polygon, but there are many corner cases and in some cases
a decision can't be made, because none of our lists match the tags.

The result is that each way is sorted in one of the following categories:

# CATEGORIES

## non-closed

Ways that are not closed, ie. first and last node IDs are different. These
are always linestrings (or they are broken, but that doesn't concern us here).

## closed

Ways that are closed, ie. first and last node IDs are equal. These are the
ways we are looking at here. The following are all subcategories of this.

## unknown

This way has only "neutral" tags, so we can't decide what this is.
There is at least one tag that doesn't match any of the "neutral",
"linestring", and "polygon" tag lists. We can't decide what this is.

## linestring

The way has an `area=no` tag or it has only "linestring" (and possibly
"neutral") tags.

## polygon

The way has an `area=yes` tag or it has only "polygon" (and possibly "neutral")
tags.

## both

The way has "linestring" and "polygon" (and possibly "neutral") tags.

## no tags

The way doesn't have any tags. This is usually because it is only used in
a multipolygon relation, but from the way tags alone we can't decide what
they are.

## error

The way has an `area` tag with a value other than `yes` or `no`. These are
definitely invalid and need to be checked and corrected.

# OPTIONS

* `--help, -h`: Print usage information.
* `--debug, -d`: Enable debug output.
* `--expressions, -e DIR`: a directory containing filter expression files.
* `--output, -o DIR`: write output to the specified directory.

This will print out some statistics to STDOUT and create several files with
names like `lp-*.osm.pbf`.

# DIAGNOSTICS

# MEMORY USAGE

# EXAMPLES

# SEE ALSO

