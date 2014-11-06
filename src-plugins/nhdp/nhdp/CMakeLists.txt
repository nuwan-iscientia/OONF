# set library parameters
SET (source nhdp.c
            nhdp_db.c
            nhdp_domain.c
            nhdp_hysteresis.c
            nhdp_interfaces.c
            nhdp_reader.c
            nhdp_writer.c)

# use generic plugin maker
oonf_create_app_plugin("nhdp" "${source}" "")
