# set library parameters
SET (source mpr.c neighbor-graph.c neighbor-graph-flooding.c neighbor-graph-routing.c selection-rfc7181.c)

# use generic plugin maker
oonf_create_app_plugin("mpr" "${source}" "")
