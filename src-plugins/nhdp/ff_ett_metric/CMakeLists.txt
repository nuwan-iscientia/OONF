# set library parameters
SET (source ff_ett_metric.c)

# use generic plugin maker
oonf_create_app_plugin("ff_ett_metric" "${source}" "")
