#include <infiniband/verbs.h>

#include "coll_remap.h"

#include "ompi/communicator/communicator.h"
#include "ompi/proc/proc.h"
#include "ompi/mca/coll/base/coll_base_functions.h"

// #include "opal/mca/common/verbs/common_verbs.h"
#include "opal/util/output.h"
#include "opal/mca/hwloc/base/base.h"

int _set_cedar_topo_info(struct ompi_communicator_t *comm, mca_coll_remap_module_t *module, int my_lid);
int _set_beluga_topo_info(struct ompi_communicator_t *comm, mca_coll_remap_module_t *module, int my_lid);
int _set_niagara_hdr_topo_info(struct ompi_communicator_t *comm, mca_coll_remap_module_t *module);
void _print_topo(int rank, int comm_size, int *arr);
int _get_ib_dev_lid();

/* assumes no oversubsciption */
int mca_coll_remap_set_proc_locality_info(struct ompi_communicator_t *comm,
                                          mca_coll_remap_module_t *module)
{
    int comm_size, rank;
    int i, ret;
    ompi_proc_t *p;

    if (NULL != module->proc_locality_arr || mca_coll_remap_component.turn_off_remap)
    {
        return OMPI_SUCCESS;
    }

    comm_size = ompi_comm_size(comm);
    rank = ompi_comm_rank(comm);

    // OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:set_proc_locality_info generating topo info"));
    module->proc_locality_arr = (int *)calloc(comm_size * comm_size, sizeof(int));

    if (OPAL_SUCCESS != opal_hwloc_base_get_topology())
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info error with opal_base_get_topology ABORTING"));
        return OMPI_ERROR;
    }

    for (i = 0; i < comm_size; i++)
    {
        p = ompi_group_peer_lookup(comm->c_local_group, i);
        module->proc_locality_arr[comm_size * rank + i] = 0;
        module->proc_locality_arr[comm_size * rank + i] += OPAL_PROC_ON_LOCAL_NODE(p->super.proc_flags);
        module->proc_locality_arr[comm_size * rank + i] += OPAL_PROC_ON_LOCAL_NUMA(p->super.proc_flags);
        module->proc_locality_arr[comm_size * rank + i] += OPAL_PROC_ON_LOCAL_SOCKET(p->super.proc_flags);
        module->proc_locality_arr[comm_size * rank + i] += OPAL_PROC_ON_LOCAL_L3CACHE(p->super.proc_flags);
        if (rank == i)
            module->proc_locality_arr[comm_size * rank + i] = -1;
    }

    if (CC_NULL != mca_coll_remap_component.cc_cluster)
    {
        int cluster = mca_coll_remap_component.cc_cluster;
        int first_device_lid = _get_ib_dev_lid();

        if (-1 == first_device_lid || NULL == mca_coll_remap_component.net_topo_input_mat)
            cluster = CC_NULL;

        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info found port with lid %u", first_device_lid));

        switch (cluster)
        {
        case CC_NIAGARA:
            _set_niagara_hdr_topo_info(comm, module);
            break;
        case CC_CEDAR:
            _set_cedar_topo_info(comm, module, first_device_lid);
            break;
        case CC_BELUGA:
            _set_beluga_topo_info(comm, module, first_device_lid);
            break;
        default:
            break;
        }
    }

    if (mca_coll_remap_component.net_topo_input_mat)
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info net_input_mat found %s", mca_coll_remap_component.net_topo_input_mat));
    }
    else
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info net_input_mat not found"));
    }

    // can't call the tuned component as it might call allgather_intra_basic_linear, which called bcast and
    // can cause in infinite loop, so any allgather could be used here, except for basic
    ret = ompi_coll_base_allgather_intra_ring(MPI_IN_PLACE, comm_size, MPI_INT, module->proc_locality_arr, comm_size, MPI_INT, comm, &(module->super));

    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info topo info set"));

    // _print_topo(rank, comm_size, module->proc_locality_arr);
    return ret;
}

void _print_topo(int rank, int comm_size, int *arr)
{
    if (rank == 0)
    {
        for (int j = 0; j < comm_size; j++)
        {
            printf("rank %d :\t", j);
            for (int k = 0; k < comm_size; k++)
            {
                printf("%d ", arr[comm_size * j + k]);
            }
            printf("\n");
        }
    }
}

int _get_ib_dev_lid()
{
    int num_devices, dev_lid;
    struct ibv_device **dev_list = ibv_get_device_list(&num_devices);
    if (num_devices == 0)
    {
        return -1;
    }
    struct ibv_context *dev_ctx = ibv_open_device(dev_list[0]);
    struct ibv_port_attr port_attr;
    ibv_query_port(dev_ctx, 1, &port_attr);
    dev_lid = port_attr.lid;

    ibv_close_device(dev_ctx);
    ibv_free_device_list(dev_list);
    return dev_lid;
}

int _set_niagara_hdr_topo_info(struct ompi_communicator_t *comm, mca_coll_remap_module_t *module)
{
    int *hostnums, rank, size, loc_sum, i, wing = 0, leaf = 0, ret;
    char node_val[5];
    char hostname_str[64];

    rank = ompi_comm_rank(comm);
    size = ompi_comm_size(comm);
    hostnums = calloc(size, sizeof(int));

    // hostname_str = getenv("HOSTNAME");
    // hostname_str = opal_gethostname();
    gethostname(hostname_str, sizeof(hostname_str));
    memcpy(node_val, hostname_str + 3, 4);
    node_val[4] = '\0';
    hostnums[rank] = atoi(node_val);

    // OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info rank %d collected hostname %s", hostnums[rank], hostname_str));

    wing = hostnums[rank] / 432;
    leaf = hostnums[rank] / 18;

    // ret = comm->c_coll->coll_allgather(MPI_IN_PLACE, 1, MPI_INT, hostnums, size, MPI_INT, comm, &(module->super));
    ret = ompi_coll_base_allgather_intra_ring(MPI_IN_PLACE, 1, MPI_INT, hostnums, 1, MPI_INT, comm, &(module->super));
    if (OMPI_SUCCESS != ret)
    {
        free(hostnums);
        return ret;
    }

    // ret = ompi_coll_base_allgather_intra_recursivedoubling(MPI_IN_PLACE, 1, MPI_INT, hostnums, 1, MPI_INT, comm, &(module->super));
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info rank %d collected hostnum %d, wing %d, leaf %d", rank, hostnums[rank], wing, leaf));

    for (i = 0; i < size; i++)
    {
        if (i == rank)
            continue;
        loc_sum = 0;
        loc_sum += (wing == hostnums[i] / 432) ? 1 : 0;
        loc_sum += (leaf == hostnums[i] / 18) ? 1 : 0;

        module->proc_locality_arr[size * rank + i] += loc_sum;
    }

    free(hostnums);
    return 0;
}

int mca_coll_remap_create_new_cached_comm(ompi_communicator_t *old_comm, int *mapping, ompi_communicator_t **new_comm)
{
    const int *origin_prio;
    int origin_prio_val, coll_remap_prio_id, tmp_prio = 0;
    const int *origin_off;
    int origin_off_val, coll_remap_off_id, tmp_off = 1;

    int rank = ompi_comm_rank(old_comm);

    mca_base_var_find_by_name("coll_remap_priority", &coll_remap_prio_id);
    mca_base_var_get_value(coll_remap_prio_id, &origin_prio, NULL, NULL);
    origin_prio_val = *origin_prio;
    mca_base_var_set_flag(coll_remap_prio_id, MCA_BASE_VAR_FLAG_SETTABLE, true);
    mca_base_var_set_value(coll_remap_prio_id, &tmp_prio, sizeof(int), MCA_BASE_VAR_SOURCE_SET, NULL);

    mca_base_var_find_by_name("coll_remap_turn_off_remap", &coll_remap_off_id);
    mca_base_var_get_value(coll_remap_off_id, &origin_off, NULL, NULL);
    origin_off_val = *origin_off;
    mca_base_var_set_flag(coll_remap_off_id, MCA_BASE_VAR_FLAG_SETTABLE, true);
    mca_base_var_set_value(coll_remap_off_id, &tmp_off, sizeof(int), MCA_BASE_VAR_SOURCE_SET, NULL);

    if (OMPI_SUCCESS != ompi_comm_split(old_comm, 0, mapping[rank], new_comm, false))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info comm_split failed"));
        return OMPI_ERROR;
    }

    mca_base_var_set_value(coll_remap_prio_id, &origin_prio_val, sizeof(int), MCA_BASE_VAR_SOURCE_SET, NULL);
    mca_base_var_get_value(coll_remap_prio_id, &origin_prio, NULL, NULL);

    mca_base_var_set_value(coll_remap_off_id, &origin_off_val, sizeof(int), MCA_BASE_VAR_SOURCE_SET, NULL);
    mca_base_var_get_value(coll_remap_off_id, &origin_off, NULL, NULL);

    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:topo_info new cached communicator created"));
    return OMPI_SUCCESS;
}

int _set_cedar_topo_info(struct ompi_communicator_t *comm, mca_coll_remap_module_t *module, int my_lid){
    // Read a file from mca_coll_remap_component.net_topo_input_mat, and increase module->proc_locality_arr based on the contents
    // The file should be an LID X LID matrix, with (1-maxhops) between each lid
    // each point of data is 3 char (2 digits and a space)
    // each row is going to be (3*num_lids + 1) chars long
    // the first line of the file is number for the total number of LIDs

    int size = ompi_comm_size(comm);
    int rank = ompi_comm_rank(comm);

    int* lid_arr = malloc(size*sizeof(int));
    lid_arr[rank] = my_lid;
    
    if(OMPI_SUCCESS != ompi_coll_base_allgather_intra_ring(MPI_IN_PLACE, 1, MPI_INT, lid_arr, 1, MPI_INT, comm, &(module->super))){
        free(lid_arr);
        return OMPI_ERROR;
    }

    // for tesiting purposes, deleteme
    // for(int i = 0; i<size; i++)
    //     lid_arr[i] = my_lid + i;
    // lid_arr[size] = my_lid;
    // for tesiting purposes, deleteme


    // by this point, lid_array will be a rank-order array of all lids
    // need to index into mca_coll_remap_component->net_topo_input_mat to find coresponding hop-byte values
    FILE *f_topo = fopen(mca_coll_remap_component.net_topo_input_mat, "r");

    int numlines, tmp;
    fscanf(f_topo, "%d\n", &numlines);
    long rank_lid_idx = (numlines*3 + 1)*(my_lid);
    fseek(f_topo, rank_lid_idx, SEEK_CUR);
    rank_lid_idx = ftell(f_topo);

    // for(int i = 0; i<size+1; i++){
    for(int i = 0; i<size; i++){
        if (i == rank)continue;
        fseek(f_topo, rank_lid_idx + 3*lid_arr[i], SEEK_SET);
        fscanf(f_topo,"%d", &tmp);
        // printf("rank_lid_idx = %d,found hop count:%d for LID:%d\n",rank_lid_idx, tmp, lid_arr[i]);

        module->proc_locality_arr[size * rank + i] += tmp;
    }

    fclose(f_topo);
    free(lid_arr);
    return MPI_SUCCESS;
}

int _set_beluga_topo_info(struct ompi_communicator_t *comm, mca_coll_remap_module_t *module, int my_lid){
    // Read a file from mca_coll_remap_component.net_topo_input_mat, and increase module->proc_locality_arr based on the contents
    // The file should be an LID X LID matrix, with (1-maxhops) between each lid
    // each point of data is 2 char (a digit and a space)
    // each row is going to be (2*num_lids + 1) chars long
    // the first line of the file is number for the total number of LIDs
    int size = ompi_comm_size(comm);
    int rank = ompi_comm_rank(comm);

    int* lid_arr = malloc(size*sizeof(int));
    lid_arr[rank] = my_lid;
    
    if(OMPI_SUCCESS != ompi_coll_base_allgather_intra_ring(MPI_IN_PLACE, 1, MPI_INT, lid_arr, 1, MPI_INT, comm, &(module->super))){
        free(lid_arr);
        return OMPI_ERROR;
    }

    FILE *f_topo = fopen(mca_coll_remap_component.net_topo_input_mat, "r");

    int numlines, tmp;
    fscanf(f_topo, "%d\n", &numlines);
    long rank_lid_idx = (numlines*2 + 1)*(my_lid);
    fseek(f_topo, rank_lid_idx, SEEK_CUR);
    rank_lid_idx = ftell(f_topo);

    // for(int i = 0; i<size+1; i++){
    for(int i = 0; i<size; i++){
        if (i == rank)continue;
        fseek(f_topo, rank_lid_idx + 2*lid_arr[i], SEEK_SET);
        fscanf(f_topo,"%d", &tmp);
        // printf("rank_lid_idx = %d,found hop count:%d for LID:%d\n",rank_lid_idx, tmp, lid_arr[i]);

        module->proc_locality_arr[size * rank + i] += tmp;
    }

    fclose(f_topo);
    free(lid_arr);
    return MPI_SUCCESS;
}
