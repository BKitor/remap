#include "opal_config.h"
#include "coll_remap.h"

const char *mca_coll_remap_component_version_string =
    "Open MPI remap collective MCA component version " OMPI_VERSION;

int ompi_coll_remap_stream = -1;

static int remap_register(void);
static int remap_open(void);

mca_coll_remap_component_t mca_coll_remap_component = {
    {
        /* First, the mca_component_t struct containing meta information
         * about the component itself */

        .collm_version = {
            MCA_COLL_BASE_VERSION_2_0_0,

            /* Component name and version */
            .mca_component_name = "remap",
            MCA_BASE_MAKE_VERSION(component, OMPI_MAJOR_VERSION, OMPI_MINOR_VERSION,
                                  OMPI_RELEASE_VERSION),

            /* Component open and close functions */
            .mca_open_component = remap_open,
            .mca_register_component_params = remap_register,
        },
        .collm_data = {/* The component is not checkpoint ready */
                       MCA_BASE_METADATA_PARAM_NONE},

        /* Initialization / querying functions */

        .collm_init_query = mca_coll_remap_init_query,
        .collm_comm_query = mca_coll_remap_comm_query,
    },
    .priority = 40,
    .select_allreduce_alg = 0,
    .select_bcast_alg = 0,
    .turn_off_remap = 0,
    .cc_cluster = 0,
    .net_topo_input_mat = NULL,
    .use_scotch = 0,
    .use_gpu_reduce = 0,
    .gpu_reduce_buffer_size = (1<<24), // default size 16MB
};

static int remap_open(void)
{
#if OPAL_ENABLE_DEBUG
    {
        int param;

        // creates a stream object for debug output
        param = mca_base_var_find("ompi", "coll", "base", "verbose");
        if (param >= 0)
        {
            const int *verbose = NULL;
            mca_base_var_get_value(param, &verbose, NULL, NULL);
            if (verbose && verbose[0] > 0)
            {
                ompi_coll_remap_stream = opal_output_open(NULL);
            }
        }
    }
#endif /* OPAL_ENABLE_DEBUG */
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:remap_open: done!"));
    return OMPI_SUCCESS;
}

static int remap_register(void)
{
    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "priority", "Priority of the remap component",
                                          MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.priority));

    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "select_allreduce_alg", "select which allreduce alg to use, same choices as tuned",
                                          MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.select_allreduce_alg));

    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "select_bcast_alg", "select which bcast alg to use, same choices as tuned",
                                          MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.select_bcast_alg));

    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "turn_off_remap", "for performace testing, passes unremaped comm instead of remapped comm",
                                          MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.turn_off_remap));

    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "cc_cluster", "compute canada cluster {null, 1:niagara, 2:mist, 3:cedar, 4:mist, 5:beluga}",
                                          MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.cc_cluster));

    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "net_topo_input_mat", "absolute localtino of matrix file with network topology",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.net_topo_input_mat));

    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "use_scotch", "use scotch to perform the mapping over heuristics",
                                          MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.use_scotch));

    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "use_gpu_reduce", "use gpu redution for allreduce collectives",
                                          MCA_BASE_VAR_TYPE_INT, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.use_gpu_reduce));

    (void)mca_base_component_var_register(&mca_coll_remap_component.super.collm_version,
                                          "gpu_reduce_buffer_size", "size of gpu buffer for gpu reduce",
                                          MCA_BASE_VAR_TYPE_SIZE_T, NULL, 0, 0, OPAL_INFO_LVL_6,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &(mca_coll_remap_component.gpu_reduce_buffer_size));
    return OMPI_SUCCESS;
}