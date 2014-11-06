# set library parameters
SET (source ff_dat_metric.c)

# use generic plugin maker
oonf_create_app_plugin("ff_dat_metric" "${source}" "")
