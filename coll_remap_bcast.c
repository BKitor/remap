#include "coll_remap.h"

#include "opal/util/output.h"
#include "ompi/mca/coll/base/coll_base_functions.h"
#include "ompi/mca/pml/pml.h"
#include "ompi/communicator/communicator.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/mca/coll/base/coll_tags.h"

int _find_closest_core(int ref_core, int comm_size, int *mapping, int *topo_info);

int mca_coll_remap_bcast_intra(void *buf, int count, struct ompi_datatype_t *dtype, int root,
                               struct ompi_communicator_t *comm, mca_coll_base_module_t *module)
{
    int alg, ret, rank, new_root = root;
    int remap_is_off = mca_coll_remap_component.turn_off_remap;
    mca_coll_remap_module_t *remap_module = (mca_coll_remap_module_t *)module;
    struct ompi_communicator_t *bcast_comm = NULL;
    struct mca_coll_base_comm_t *base_data_swp = NULL;

    rank = ompi_comm_rank(comm);

    alg = (mca_coll_remap_component.select_bcast_alg) ? mca_coll_remap_component.select_bcast_alg : remap_bcast_pick_alg(count, dtype, comm);

    if (alg < 0 || alg >= REMAP_BCAST_ALG_COUNT)
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast alg %d isn't valid, chose between 0 and %d, ABORTING",
                     alg, (REMAP_BCAST_ALG_COUNT - 1)));
        goto bcast_abort;
    }
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast alg %d selected", alg));

    bcast_comm = (remap_is_off) ? comm : remap_module->cached_bcast_comm[alg];

    if (!remap_is_off && MPI_SUCCESS != mca_coll_remap_set_proc_locality_info(comm, remap_module))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast remap_set_proc_locality_info failed, ABORTING"));
        goto bcast_abort;
    }

    if (NULL == bcast_comm && !remap_is_off)
    {
        /* cached module doesn't exist, make it exist */
        switch (alg)
        {
        case REMAP_BCAST_ALG_PIPELINE:
            ret = remap_bcast_pipeline_remap(comm, remap_module, &bcast_comm);
            if (OMPI_SUCCESS != ret)
            {
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast pipeline remaping failed, ABORTING"));
                goto bcast_abort;
            }
            remap_module->cached_bcast_comm[alg] = bcast_comm;
            break;
        case REMAP_BCAST_ALG_BIN_TREE:
            ret = remap_bcast_bintree_remap(comm, remap_module, &bcast_comm);
            if (OMPI_SUCCESS != ret)
            {
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast pipeline remaping failed, ABORTING"));
                goto bcast_abort;
            }
            remap_module->cached_bcast_comm[alg] = bcast_comm;
            break;
        case REMAP_BCAST_ALG_BINOMIAL:
            ret = remap_bcast_binomial_remap(comm, remap_module, &bcast_comm);
            if (OMPI_SUCCESS != ret)
            {
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast binomial remaping failed, ABORTING"));
                goto bcast_abort;
            }
            remap_module->cached_bcast_comm[alg] = bcast_comm;
            break;
        case REMAP_BCAST_ALG_KNOMIAL:
            OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast performing knomial mapping"));
            ret = remap_bcast_knomial_remap(comm, remap_module, &bcast_comm, 4);
            if (OMPI_SUCCESS != ret)
            {
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast knomial remapping failed, ABORTIGN"));
                goto bcast_abort;
            }
            remap_module->cached_bcast_comm[alg] = bcast_comm;
            break;
        case REMAP_BCAST_ALG_SCATTER_ALLGATHER:
            OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast performing scatter_allgather mapping"));
            ret = remap_bcast_scatter_allgather_remap(comm, remap_module, &bcast_comm);
            if (OMPI_SUCCESS != ret)
            {
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast scatter_allgather remapping failed, ABORTIGN"));
                goto bcast_abort;
            }
            remap_module->cached_bcast_comm[alg] = bcast_comm;
            break;
        default:
            OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast there is no mapping for the selected alg, ABORTING"));
            goto bcast_abort;
            break;
        }
    }
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast rank:%d root:%d remap_is_off:%d", rank, root, remap_is_off));

    // remaping is performed assuming root is 0, so make sure root has the data
    if (root != 0 && !remap_is_off)
    {
        new_root = 0;
        if (root == rank)
        {
            // send buff to 0
            MCA_PML_CALL(send(buf, count, dtype, 0, MCA_COLL_BASE_TAG_BCAST,
                              MCA_PML_BASE_SEND_STANDARD, comm));
        }
        else if (0 == rank)
        {
            //recieve buff from root
            MCA_PML_CALL(recv(buf, count, dtype, root, MCA_COLL_BASE_TAG_BCAST,
                              comm, MPI_STATUS_IGNORE));
        }
    }

    // if remapping is performed, need to swap out the base_data
    if (!remap_is_off)
    {
        base_data_swp = remap_module->super.base_data;
        remap_module->super.base_data = remap_module->cached_base_data;
    }

    switch (alg)
    {
    case REMAP_BCAST_ALG_LINEAR:
        ret = ompi_coll_base_bcast_intra_basic_linear(buf, count, dtype, new_root, bcast_comm, module);
        break;
    case REMAP_BCAST_ALG_CHAIN:
        ret = ompi_coll_base_bcast_intra_chain(buf, count, dtype, new_root, bcast_comm, module, 0, 0);
        break;
    case REMAP_BCAST_ALG_PIPELINE:
        ret = ompi_coll_base_bcast_intra_pipeline(buf, count, dtype, new_root, bcast_comm, module, 0);
        break;
    case REMAP_BCAST_ALG_SPLIT_BIN_TREE:
        ret = ompi_coll_base_bcast_intra_split_bintree(buf, count, dtype, new_root, bcast_comm, module, 0);
        break;
    case REMAP_BCAST_ALG_BIN_TREE:
        // allocate space for a cached binary topo tree within the remap_module
        // swap the topo that the collective will call, for the remap one
        ret = ompi_coll_base_bcast_intra_bintree(buf, count, dtype, new_root, bcast_comm, module, 0);
        // swap back
        break;
    case REMAP_BCAST_ALG_BINOMIAL:
        ret = ompi_coll_base_bcast_intra_binomial(buf, count, dtype, new_root, bcast_comm, module, 0);
        break;
    case REMAP_BCAST_ALG_KNOMIAL:
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast running knomial algorihtm"));
        // radix is set to 4, because that's the default value of coll_tuned_bcast_knomial_radix
        ret = ompi_coll_base_bcast_intra_knomial(buf, count, dtype, new_root, bcast_comm, module, 0, 4);
        break;
    case REMAP_BCAST_ALG_SCATTER_ALLGATHER:
        ret = ompi_coll_base_bcast_intra_scatter_allgather(buf, count, dtype, new_root, bcast_comm, module, 0);
        break;
    case REMAP_BCAST_ALG_SCATTER_ALLGATHER_RING:
        ret = ompi_coll_base_bcast_intra_scatter_allgather_ring(buf, count, dtype, new_root, bcast_comm, module, 0);
        break;
    default:
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast you seem to have reached a part of code that shouldn't be possible, what have you done? ABORTING"));
        goto bcast_abort;
        break;
    }

    // if a swap has been performed, swap back
    if (NULL != base_data_swp)
    {
        remap_module->super.base_data = base_data_swp;
    }

    return ret;

bcast_abort:
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast failed, passing to FALLBACK"));
    return remap_module->fallback_bcast_fn(buf, count, dtype, root, comm,
                                           remap_module->fallback_bcast_module);
}

int _find_closest_core(int ref_core, int comm_size, int *mapping, int *topo_info)
{
    int j, closest_core = ref_core;
    for (j = 0; j < comm_size; j++)
    {
        if (mapping[j] >= 0)
            continue;
        if (topo_info[comm_size * ref_core + j] > topo_info[comm_size * ref_core + closest_core])
            closest_core = j;
    }
    return closest_core;
}

int remap_bcast_pipeline_remap(struct ompi_communicator_t *old_comm,
                               mca_coll_remap_module_t *module,
                               struct ompi_communicator_t **new_comm)
{
    int comm_size, rank, pinned_core, closest_core, i, j;
    int *mapping, *topo_info = module->proc_locality_arr;

    comm_size = ompi_comm_size(old_comm);
    rank = ompi_comm_rank(old_comm);

    mapping = malloc(sizeof(int) * comm_size);
    for (i = 0; i < comm_size; i++)
        mapping[i] = -1;

    mapping[0] = 0;
    pinned_core = 0;

    for (i = 1; i < comm_size; i++)
    {
        closest_core = pinned_core;

        for (j = 0; j < comm_size; j++)
        {
            if (mapping[j] >= 0)
                continue;
            if (topo_info[comm_size * pinned_core + j] > topo_info[comm_size * pinned_core + closest_core])
                closest_core = j;
        }

        mapping[closest_core] = i;
        pinned_core = closest_core;
    }

    if (OMPI_SUCCESS != mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast PIPELINE coll_remap_create_new_cached_comm failed"));
        free(mapping);
        return OMPI_ERROR;
    }

    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast PIPELINE rank %d on core %d", ompi_comm_rank(*new_comm), rank));
    free(mapping);
    return OMPI_SUCCESS;
}

void _rec_bin_mapping(int rr, int rc, int comm_size, int *topo_info, int *mapping);

// this mapping assumes rank 0 and a power of 2 comm_size is the root of the broadcast
int remap_bcast_binomial_remap(struct ompi_communicator_t *old_comm,
                               mca_coll_remap_module_t *module,
                               struct ompi_communicator_t **new_comm)
{
    int comm_size, rank;
    int *mapping, *topo_info = module->proc_locality_arr;

    comm_size = ompi_comm_size(old_comm);
    rank = ompi_comm_rank(old_comm);

    mapping = malloc(sizeof(int) * comm_size);
    for (int i = 0; i < comm_size; i++)
        mapping[i] = -1;
    mapping[0] = 0;

    _rec_bin_mapping(0, 0, comm_size, topo_info, mapping);

    if (OMPI_SUCCESS != mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast BINOMIAL coll_remap_create_new_cached_comm failed"));
        free(mapping);
        return OMPI_ERROR;
    }

    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast BINOMIAL rank %d on core %d", ompi_comm_rank(*new_comm), rank));
    free(mapping);
    return OMPI_SUCCESS;
}

void _rec_bin_mapping(int rr, int rc, int comm_size, int *topo_info, int *mapping)
{
    int mask = comm_size >> 1, ref_rank = rr, ref_core = rc;
    int proc_to_bind, closest_core;

    while (((ref_rank & mask) == 0) && (mask > 0))
    {
        proc_to_bind = ref_rank + mask;
        closest_core = _find_closest_core(ref_core, comm_size, mapping, topo_info);

        mapping[closest_core] = proc_to_bind;

        _rec_bin_mapping(proc_to_bind, closest_core, comm_size, topo_info, mapping);
        mask >>= 1;
    }
    return;
}

void _rec_knom_mapping(int rr, int rc, int world_size, int comm_size, int radix, int *topo_info, int *mapping);

int remap_bcast_knomial_remap(struct ompi_communicator_t *old_comm,
                              mca_coll_remap_module_t *module,
                              struct ompi_communicator_t **new_comm, int radix)
{
    int comm_size, rank;
    int *mapping, *topo_info = module->proc_locality_arr;

    comm_size = ompi_comm_size(old_comm);
    rank = ompi_comm_rank(old_comm);

    mapping = malloc(sizeof(int) * comm_size);
    for (int i = 0; i < comm_size; i++)
        mapping[i] = -1;
    mapping[0] = 0;

    _rec_knom_mapping(0, 0, comm_size, comm_size, radix, topo_info, mapping);

    if (OMPI_SUCCESS != mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast KNOMIAL create_new_cached_comm faied"));
        free(mapping);
        return OMPI_ERROR;
    }
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast KNOMIAL rank %d on core %d", ompi_comm_rank(*new_comm), rank));
    free(mapping);
    return OMPI_SUCCESS;
}

void _rec_knom_mapping(int rr, int rc, int world_size, int comm_size, int radix, int *topo_info, int *mapping)
{
    int mask = 1, proc_to_bind, i, closest_core, trimmed_world_size;

    for (i = rr + 1; i < rr + radix; i++)
    {
        if (i < world_size)
        {
            closest_core = _find_closest_core(rc, comm_size, mapping, topo_info);
            mapping[closest_core] = i;
        }
    }

    while ((mask * radix) < world_size)
    {
        for (i = 1; i < radix; i++)
        {
            proc_to_bind = rr + (mask * radix * i);

            if (proc_to_bind < world_size)
            {
                closest_core = _find_closest_core(rc, comm_size, mapping, topo_info);
                mapping[closest_core] = proc_to_bind;
                trimmed_world_size = proc_to_bind + radix * mask;
                _rec_knom_mapping(proc_to_bind, closest_core,
                                  (trimmed_world_size < world_size) ? trimmed_world_size : world_size,
                                  comm_size, radix, topo_info, mapping);
            }
        }
        mask *= radix;
    }
    return;
}

// This is the recursive doubling heuristic form Hessam's work
int remap_bcast_scatter_allgather_remap(struct ompi_communicator_t *old_comm,
                                        mca_coll_remap_module_t *module,
                                        struct ompi_communicator_t **new_comm)
{
    int comm_size, rank, ref_rank, i, j, mask, new_rank, new_core, num_mappings;
    int *topo_info = module->proc_locality_arr, *mapping;

    rank = ompi_comm_rank(old_comm);
    comm_size = ompi_comm_size(old_comm);
    mapping = malloc(comm_size * sizeof(int));

    for (int i = 0; i < comm_size; i++)
        mapping[i] = -1;
    mapping[0] = 0;
    ref_rank = 0;
    mask = comm_size / 2;

    num_mappings = 0;
    for (i = 1; i < comm_size; i++)
    {
        for (j = 0; j < comm_size; j++)
        {
            if (mapping[j] == (ref_rank ^ mask))
            {
                j = -1;
                mask /= 2;
            }
        }
        new_rank = ref_rank ^ mask;
        new_core = _find_closest_core(ref_rank, comm_size, mapping, topo_info);
        mapping[new_core] = new_rank;
        num_mappings++;

        if (num_mappings == 2)
        {
            ref_rank = new_rank;
            mask = comm_size / 2;
            num_mappings = 0;
        }
    }

    if (OMPI_SUCCESS != mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast SC_AG coll_remap_create_new_cached_comm failed"));
        free(mapping);
        return OMPI_ERROR;
    }
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast SC_AG rank %d mapped to core %d", ompi_comm_rank(*new_comm), rank));
    free(mapping);
    return OMPI_SUCCESS;
}

void _rec_bintree_mapping(int rr, int rc, int comm_size, int *topo_info, int *mapping);
int _rec_bintree_mapping_2(int rr, int nxt_c, int comm_size, int *topo_info, int *mapping);

int remap_bcast_bintree_remap(struct ompi_communicator_t *old_comm,
                              mca_coll_remap_module_t *module,
                              struct ompi_communicator_t **new_comm)
{
    int comm_size, rank;
    int *topo_info = module->proc_locality_arr, *mapping;

    rank = ompi_comm_rank(old_comm);
    comm_size = ompi_comm_size(old_comm);
    mapping = malloc(comm_size * sizeof(int));

    for (int i = 0; i < comm_size; i++)
        mapping[i] = -1;
    // mapping[0] = 0;

    // _rec_bintree_mapping(0, 0, comm_size, topo_info, mapping);
    _rec_bintree_mapping_2(0, 0, comm_size, topo_info, mapping);

    if (OMPI_SUCCESS != mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast BINTREE coll_remap_create_new_cached_comm failed"));
        free(mapping);
        return OMPI_ERROR;
    }
    // OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast BINTREE rank %d mapped to core %d", rank, mapping[rank]));
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:bcast BINTREE rank %d bound to core %d", ompi_comm_rank(*new_comm), rank));
    free(mapping);
    return OMPI_SUCCESS;
}

static int pown(int fanout, int num)
{
    int j, p = 1;
    if (num < 0)
        return 0;
    if (1 == num)
        return fanout;
    if (2 == fanout)
    {
        return p << num;
    }
    else
    {
        for (j = 0; j < num; j++)
        {
            p *= fanout;
        }
    }
    return p;
}

static int calculate_level(int fanout, int rank)
{
    int level, num;
    if (rank < 0)
        return -1;
    for (level = 0, num = 0; num <= rank; level++)
    {
        num += pown(fanout, level);
    }
    return level - 1;
}

void _rec_bintree_mapping(int rr, int rc, int comm_size, int *topo_info, int *mapping)
{
    int l, d, c1, c2, tmp_core;
    l = calculate_level(2, rr);
    d = pown(2, l);
    c1 = rr + d;
    c2 = rr + 2 * d;
    if (c1 >= comm_size)
        return;
    tmp_core = _find_closest_core(rc, comm_size, mapping, topo_info);
    mapping[tmp_core] = c1;
    _rec_bintree_mapping(c1, tmp_core, comm_size, topo_info, mapping);
    // printf("tmp_core:%d, rr:%d, c1:%d \n", tmp_core, rr, c1);

    if (c2 >= comm_size)
        return;
    tmp_core = _find_closest_core(rc, comm_size, mapping, topo_info);
    mapping[tmp_core] = c2;
    _rec_bintree_mapping(c2, tmp_core, comm_size, topo_info, mapping);
    // printf("tmp_core:%d, rr:%d, c2:%d \n", tmp_core, rr, c2);
}

int _rec_bintree_mapping_2(int rr, int nxt_c, int comm_size, int *topo_info, int *mapping)
{
    int l, d, c1, c2, core_tmp = nxt_c;
    l = calculate_level(2, rr);
    d = pown(2, l);
    c1 = rr + d;
    c2 = rr + 2 * d;
    if (c1 < comm_size)
        core_tmp = _rec_bintree_mapping_2(c1, core_tmp, comm_size, topo_info, mapping);
    mapping[core_tmp] = rr;
    core_tmp = _find_closest_core(core_tmp, comm_size, mapping, topo_info);
    if (c2 < comm_size)
        core_tmp = _rec_bintree_mapping_2(c2, core_tmp, comm_size, topo_info, mapping);
    return core_tmp;
}

int remap_bcast_pick_alg(int count, struct ompi_datatype_t *datatype,
                         struct ompi_communicator_t *comm)
{
    size_t total_dsize, dsize;
    int communicator_size, alg;
    communicator_size = ompi_comm_size(comm);

    ompi_datatype_type_size(datatype, &dsize);
    total_dsize = dsize * (unsigned long)count;

    /** Algorithms:
     *  {1, "basic_linear"},
     *  {2, "chain"},
     *  {3, "pipeline"},
     *  {4, "split_binary_tree"},
     *  {5, "binary_tree"},
     *  {6, "binomial"},
     *  {7, "knomial"},
     *  {8, "scatter_allgather"},
     *  {9, "scatter_allgather_ring"},
     */
    if (communicator_size < 4)
    {
        if (total_dsize < 2)
        {
            alg = 9;
        }
        else if (total_dsize < 32)
        {
            alg = 3;
        }
        else if (total_dsize < 256)
        {
            alg = 5;
        }
        else if (total_dsize < 512)
        {
            alg = 3;
        }
        else if (total_dsize < 1024)
        {
            alg = 7;
        }
        else if (total_dsize < 32768)
        {
            alg = 1;
        }
        else if (total_dsize < 131072)
        {
            alg = 5;
        }
        else if (total_dsize < 262144)
        {
            alg = 2;
        }
        else if (total_dsize < 524288)
        {
            alg = 1;
        }
        else if (total_dsize < 1048576)
        {
            alg = 6;
        }
        else
        {
            alg = 5;
        }
    }
    else if (communicator_size < 8)
    {
        if (total_dsize < 2)
        {
            alg = 8;
        }
        else if (total_dsize < 64)
        {
            alg = 5;
        }
        else if (total_dsize < 128)
        {
            alg = 6;
        }
        else if (total_dsize < 2048)
        {
            alg = 5;
        }
        else if (total_dsize < 8192)
        {
            alg = 6;
        }
        else if (total_dsize < 1048576)
        {
            alg = 1;
        }
        else
        {
            alg = 2;
        }
    }
    else if (communicator_size < 16)
    {
        if (total_dsize < 8)
        {
            alg = 7;
        }
        else if (total_dsize < 64)
        {
            alg = 5;
        }
        else if (total_dsize < 4096)
        {
            alg = 7;
        }
        else if (total_dsize < 16384)
        {
            alg = 5;
        }
        else if (total_dsize < 32768)
        {
            alg = 6;
        }
        else
        {
            alg = 1;
        }
    }
    else if (communicator_size < 32)
    {
        if (total_dsize < 4096)
        {
            alg = 7;
        }
        else if (total_dsize < 1048576)
        {
            alg = 6;
        }
        else
        {
            alg = 8;
        }
    }
    else if (communicator_size < 64)
    {
        if (total_dsize < 2048)
        {
            alg = 6;
        }
        else
        {
            alg = 7;
        }
    }
    else if (communicator_size < 128)
    {
        alg = 7;
    }
    else if (communicator_size < 256)
    {
        if (total_dsize < 2)
        {
            alg = 6;
        }
        else if (total_dsize < 128)
        {
            alg = 8;
        }
        else if (total_dsize < 16384)
        {
            alg = 5;
        }
        else if (total_dsize < 32768)
        {
            alg = 1;
        }
        else if (total_dsize < 65536)
        {
            alg = 5;
        }
        else
        {
            alg = 7;
        }
    }
    else if (communicator_size < 1024)
    {
        if (total_dsize < 16384)
        {
            alg = 7;
        }
        else if (total_dsize < 32768)
        {
            alg = 4;
        }
        else
        {
            alg = 7;
        }
    }
    else if (communicator_size < 2048)
    {
        if (total_dsize < 524288)
        {
            alg = 7;
        }
        else
        {
            alg = 8;
        }
    }
    else if (communicator_size < 4096)
    {
        if (total_dsize < 262144)
        {
            alg = 7;
        }
        else
        {
            alg = 8;
        }
    }
    else
    {
        if (total_dsize < 8192)
        {
            alg = 7;
        }
        else if (total_dsize < 16384)
        {
            alg = 5;
        }
        else if (total_dsize < 262144)
        {
            alg = 7;
        }
        else
        {
            alg = 8;
        }
    }

    return alg;
}
