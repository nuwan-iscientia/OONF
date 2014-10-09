# set library name
SET (source "layer2_generator.c")

# use generic plugin maker
oonf_create_plugin("layer2_generator" "${source}" "" "")
