
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

# OUTPUT AND REPORTS

The following problems will be found for any relations:

* Large relations (`large`): Relations with more than 1000 members. This is
  allowed but quite unusual.
* Relations without members (`no-members`): A relation without members doesn't
  make any sense.
* Relations without tags (`no-tag`): A relation without tags isn't very useful.
* Relations without a `type` tag (`no-type-tag`): Relations should have a
  `type` tag, but strictly speaking not an error.
* Relations with only `type` tag (`only-type-tag`): Relations with a `type`
  tag but nothing else are unusual but not necessarily a problem.
* Relations with themselves as member (`references-self`): This will probably
  break a lot of software that doesn't expect this and there isn't really a
  use for allowing this.

The following problems will be found for multipolygon relations:

* Relation has a node member (`multipolygon_node_member`): Multipolygon
  relations should only have way members.
* Relation has a relation member (`multipolygon_relation_member`): Multipolygon
  relations should only have way members.
* Unknown member role (`multipolygon_unknown_role`): Only roles `inner` or
  `outer` are allowed.
* Empty member role (`multipolygon_empty_role`): Roles should always be `inner`
  or `outer`.
* Relation has an `area` tag (`multipolygon_area_tag`): Multipolygon relations
  are always areas, so this is superfluous.
* Relation has `boundary=administrative` tag
  (`multipolygon_boundary_administrative_tag`): If this is a boundary, it
  should be tagged as `type=boundary` instead.
* Relation has a `boundary` tag (with a value other than `administrative`)
  (`multipolygon_boundary_other_tag`): If this is a boundary, it should be
  tagged as `type=boundary` instead.
* Old-style multipolygon (`multipolygon_old_style`): This should definitely
  be converted to new style.
* Multipolygon has only a single way member (`multipolygon_single_way`): This
  probably means the multipolygon should be modelled as a closed way instead.
* Duplicate ways in multipolygon (`multipolygon_duplicate_way`): This should
  definitely be fixed.

The following problems will be found for boundary relations:

* Empty member role (`boundary_empty_role`): Roles should always be set.
* Duplicate ways in boundary (`boundary_duplicate_way`): This should definitely
  be fixed.
* Relation has an `area` tag (`boundary_area_tag`): Boundary relations are
  always sort-of areas, so this is superfluous.
* No `boundary` tags (`boundary_no_boundary_tag`): Boundary relations should
  always have a `boundary` tag.

How often these problems are found is reported in the `stats` table of the
output file `stats-relation-problems.db`. In addition OSM files are written
out for these, once with only the relation, once (with the infix `-all`)
including the members.

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

