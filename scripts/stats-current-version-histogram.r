
# Can be used with the output of osp-stats-basic

library(DBI)
library(ggplot2)

theme_set(theme_grey(base_size = 24))

db <- dbConnect(RSQLite::SQLite(), "scvh.db")

nodes <- dbGetQuery(db, "SELECT * FROM hist_versions WHERE object_type = 'n' ORDER BY versions")
ways <- dbGetQuery(db, "SELECT * FROM hist_versions WHERE object_type = 'w' ORDER BY versions")
relations <- dbGetQuery(db, "SELECT * FROM hist_versions WHERE object_type = 'r' ORDER BY versions")

qplot(versions, num, data = nodes, colour = I("steelblue"), main = "Nodes", xlab = "Versions", ylab = "Number of nodes with this version")
qplot(versions, num, data = ways, colour = I("steelblue"), main = "Ways", xlab = "Versions", ylab = "Number of ways with this version")
qplot(versions, num, data = relations, colour = I("steelblue"), main = "Relations", xlab = "Versions", ylab = "Number of relations with this version")

