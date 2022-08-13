
# NAME

osp-find-relation-problems - Find relations with problems

# SYNOPSIS

**osp-find-relation-problems** \*[OPTIONS\*] *OSM-FILE* *OUTPUT-DIR*

# DESCRIPTION

Finds several problems with relations.

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

-q, \--quiet
:   Work quietly.

# DIAGNOSTICS

# MEMORY USAGE

# EXAMPLES

# SEE ALSO

