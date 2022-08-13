
# NAME

osp-find-orphans - Find objects that are unreferenced and untagged

# SYNOPSIS

**osp-find-orphans** \[*OPTIONS*\] *OSM-FILE* *OUTPUT-DIR*

# DESCRIPTION

"Orphans" are OSM objects (nodes, ways, or relations) that have no tags and
that are not referenced by any other objects. "No tags" in this case also means
objects that have only `source` or `created_by` tags, because they don't say
anything about what an object actually *is*. Orphan objects are always an
error, but, for ways and relations, their members might tell you something
about their intended use.

Do not trust the output of this command when run on an extract! The extract
might not contain all objects referencing the objects in the extract.

# OPTIONS

-a, \--min-age=DAYS
:   Only include objects at least DAYS days old. Can not be used together with
    \--before.

-b, \--before=TIMESTAMP
:   Only include objects changed last before this time
    (format: `yyyy-mm-ddThh:mm:ssZ`). Can not be used together with \--min-age.

-h, \--help
:   Show usage help.

-q, --quiet
:   Work quietly.

-u, \--untagged-only
:   Untagged objects only.

-U, \--no-untagged
:   No untagged objects.

# DIAGNOSTICS

# MEMORY USAGE

# EXAMPLES

# SEE ALSO

