#include "coll_remap.h"

#include "opal/util/output.h"

#include "ompi/mca/coll/base/coll_base_functions.h"
#include "ompi/communicator/communicator.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/op/op.h"


/*
1. based on the comm, decide which algorithm to run, should be similar to tuned's decision
2. check if a cached communicator exists on the module
    2a. if exists, ur gucci
    2b. if it doesn't exist, create and cache the new comm
3. call the alg with the cached comm

    going to need a way to shuffle the mem, will fix when get there
    
*/
int mca_coll_remap_allreduce_intra(const void *sbuf, void *rbuf, int count,
                                       struct ompi_datatype_t *dtype,
                                       struct ompi_op_t *op,
                                       struct ompi_communicator_t *comm,
                                       mca_coll_base_module_t *module)
{
    mca_coll_remap_module_t *remap_module = (mca_coll_remap_module_t*) module;
    ompi_communicator_t *ar_comm = NULL;
    int alg, ret;

    if(!ompi_op_is_commute(op)){
        /* Can only do commutative atm */
        goto ar_abort;
    }

    alg = (mca_coll_remap_component.select_allreduce_alg)? mca_coll_remap_component.select_allreduce_alg : remap_allreduce_pick_alg(count, dtype, comm);
    if(alg < 0 || alg >= REMAP_ALLREDUCE_ALG_COUNT){
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce alg %d isn't a valid alg, choose between 0 and %d, ABORTING", 
                                             alg, (REMAP_ALLREDUCE_ALG_COUNT - 1)));
        goto ar_abort;
    }

    // if remap is off, just stick with the communicator passed in
    ar_comm = (mca_coll_remap_component.turn_off_remap)?comm:remap_module->cached_allreduce_comm[alg];

    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce alg chosen:%d", alg));

    /* setup process locality info on remap_module */
    if ( MPI_SUCCESS != mca_coll_remap_set_proc_locality_info(comm, remap_module)){
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce remap_set_proc_locality_info failed, ABORTING"));
        goto ar_abort;
    }

    if(NULL == ar_comm){
        /* cached module doesn't exist, make it exist */
        switch (alg){
        case REMAP_ALLREDUCE_ALG_LINEAR:
            ret = remap_allreduce_linear_remap(comm, remap_module, &ar_comm);
            if(OMPI_SUCCESS != ret){
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce linear remap didn't work, ABORTING"));
                goto ar_abort;
            }
            remap_module->cached_allreduce_comm[alg] = ar_comm;
        break;
        case REMAP_ALLREDUCE_ALG_NON_OVERLAP:
            ar_comm = comm;
        break;
        case REMAP_ALLREDUCE_ALG_RECURSIVE_DOUBLING:
            ret = remap_allreduce_rdouble_remap(comm, remap_module, &ar_comm);
            if(OMPI_SUCCESS != ret){
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce rdouble remap didn't work, ABORTING"));
                goto ar_abort;
            }
            remap_module->cached_allreduce_comm[alg] = ar_comm;
        break;
        case REMAP_ALLREDUCE_ALG_SEGMENTED_RING: 
        case REMAP_ALLREDUCE_ALG_RING: 
            ret = remap_allreduce_ring_remap(comm, remap_module, &ar_comm);
            if(OMPI_SUCCESS != ret){
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce ring remap didn't work, ABORTING"));
                goto ar_abort;
            }
            remap_module->cached_allreduce_comm[alg] = ar_comm;
        break;
        case REMAP_ALLREDUCE_ALG_RABENSEIFNER:
            ret = remap_allreduce_raben_remap(comm, remap_module, &ar_comm);
            if(OMPI_SUCCESS != ret){
                OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce raben remap didn't work, ABORTING"));
                goto ar_abort;
            }
            remap_module->cached_allreduce_comm[alg] = ar_comm;
        break;
        default:
            OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce there is no mapping for the selected alg, ABORTING"));
            goto ar_abort;
        break;
        }
    }

    
    switch(alg){
        case REMAP_ALLREDUCE_ALG_LINEAR:
            ret = ompi_coll_base_allreduce_intra_basic_linear(sbuf, rbuf, count, dtype, op,
                                                            ar_comm, ar_comm->c_coll->coll_allreduce_module);
        break;
        case REMAP_ALLREDUCE_ALG_NON_OVERLAP:
            ret = ompi_coll_base_allreduce_intra_nonoverlapping(sbuf, rbuf, count, dtype, op,
                                                            ar_comm, ar_comm->c_coll->coll_allreduce_module);
        break;
        case REMAP_ALLREDUCE_ALG_RECURSIVE_DOUBLING:
            ret = ompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, dtype, op,
                                                                ar_comm, ar_comm->c_coll->coll_allreduce_module);
        break;
        case REMAP_ALLREDUCE_ALG_RING:
        case REMAP_ALLREDUCE_ALG_SEGMENTED_RING:
            ret = ompi_coll_base_allreduce_intra_ring(sbuf, rbuf, count, dtype, op, ar_comm, 
                                                    ar_comm->c_coll->coll_allreduce_module);
        break;
        case REMAP_ALLREDUCE_ALG_RABENSEIFNER:
            ret = ompi_coll_base_allreduce_intra_redscat_allgather(sbuf, rbuf, count, dtype, op,
                                                    ar_comm, ar_comm->c_coll->coll_allreduce_module);
        break;
        default:
            OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce you seem to have reached a part of code that shouldn't be possible, what have you done? ABORTING"));
            goto ar_abort;
        break;
    }
   

    /*
    ret = ompi_coll_tuned_allreduce_intra_do_this(sbuf, rbuf, count, dtype, op,
                                                  ar_comm, module, alg, 0, 0);
    */

    return ret;

    ar_abort:
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce failed, passing to FALLBACK"));
    return remap_module->fallback_allreduce_fn(sbuf, rbuf, count, dtype,
                                                op, comm,
                                                remap_module->fallback_allreduce_module);
}

/* 
    creates a new communicator and stores it in new comm 

    since thinking is hard, I'm starting with an implimentation of Hessam's alg
    from "Topology-Aware rank Reordering for MPI Collectives"
*/
int remap_allreduce_ring_remap(ompi_communicator_t *old_comm,
                mca_coll_remap_module_t *module, ompi_communicator_t **new_comm)
{
    int comm_size, i, j, rank, pinned_rank;
    rank = ompi_comm_rank(old_comm);
    comm_size = ompi_comm_size(old_comm);
    int *topo_info=module->proc_locality_arr, *mapping;

    mapping = malloc(sizeof(int)*comm_size);
    for(i = 0; i<comm_size; i++) mapping[i]=-1;
    mapping[0] = 0;
    pinned_rank = 0;

    for(i = 1; i<comm_size; i++){
        int closest = pinned_rank;
        // find closest core to pin rank
        for(j = 0; j<comm_size; j++){
            if( mapping[j] >= 0)continue;
            if( topo_info[comm_size*pinned_rank+j] > topo_info[comm_size*pinned_rank+closest] )
                closest = j;
        }

        mapping[closest] = i;
        pinned_rank = closest;

    }

    if(mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm)){
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce RING coll_remap_create_new_cached_comm failed"));
        free(mapping);
        return OMPI_ERROR;
    }

    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce RING rank %d on core %d", ompi_comm_rank(*new_comm), rank));

    free(mapping);
    return OMPI_SUCCESS;
}

/*
    This is Hessam's implimentation for recursive doubleing from 'Topo Aware Rank Reordering'
    It assumes numprocs is a power of 2
    *** this alg assumes numprocs is pow 2 ***
*/
int remap_allreduce_rdouble_remap(ompi_communicator_t *old_comm,
                                  mca_coll_remap_module_t *module,
                                  ompi_communicator_t **new_comm)
{ 
    int comm_size, rank, ref_rank, ref_core, tmp_int_i, closest_core, num_rank_mappings, new_rank, i, j;
    int *topo_info=module->proc_locality_arr, *mapping;
    rank = ompi_comm_rank(old_comm);
    comm_size = ompi_comm_size(old_comm);

    mapping = malloc(comm_size*sizeof(int));
    for(i = 1; i<comm_size; i++)mapping[i] = -1;
    mapping[0] = 0; 
    ref_rank=0;
    ref_core = 0;
    tmp_int_i = comm_size/2;
    num_rank_mappings = 0;

    for(i=1; i<comm_size; i++){
        // find next rank to map
        for(j = 0; j<comm_size; j++){
            if(mapping[j] == (ref_rank^tmp_int_i)){
                j = -1;
                tmp_int_i /= 2;
            }
        }

        new_rank = ref_rank ^ tmp_int_i;

        // find closest mapping
        closest_core = ref_core;
        for(j = 0; j<comm_size; j++){ 
            if(mapping[j]>=0) continue;
            if(topo_info[comm_size*ref_core+j] > topo_info[comm_size*ref_core+closest_core])
                closest_core = j;
        }

        mapping[closest_core] = new_rank;
        num_rank_mappings++;

        if (num_rank_mappings == 2){
            ref_rank = new_rank;
            ref_core = closest_core;
            tmp_int_i = comm_size/2;
            num_rank_mappings = 0;
        }
    }

    if (OMPI_SUCCESS != mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm)){
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce RDOUBLE coll_remap_create_new_cached_comm failed"));
        free(mapping);
        return OMPI_ERROR;
    }
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce RDOUBLE rank %d on core %d", ompi_comm_rank(*new_comm), rank));
    free(mapping);
    return OMPI_SUCCESS;

}

/*
    Hessam's RDouble in reverse should theoretically provide better mapping for raben
    in practice, compared to a ring, it only provides a more optimal mapping in edge cases

    *** this alg assumes numprocs is pow 2 ***
*/
int remap_allreduce_raben_remap(ompi_communicator_t *old_comm,
                                  mca_coll_remap_module_t *module,
                                  ompi_communicator_t **new_comm)
{ 
    int comm_size, rank, ref_rank, ref_core, tmp_int_i, closest_core, num_rank_mappings, new_rank, i, j;
    int *topo_info=module->proc_locality_arr, *mapping;
    rank = ompi_comm_rank(old_comm);
    comm_size = ompi_comm_size(old_comm);

    mapping = malloc(comm_size*sizeof(int));
    for(i = 1; i<comm_size; i++)mapping[i] = -1;
    mapping[0] = 0; 
    ref_rank=0;
    ref_core = 0;
    tmp_int_i = 1;
    num_rank_mappings = 0;

    for(i=1; i<comm_size; i++){
        // find next rank to map
        for(j = 0; j<comm_size; j++){
            if(mapping[j] == (ref_rank^tmp_int_i)){
                j = -1;
                tmp_int_i <<= 1 ;
            }
        }

        new_rank = ref_rank ^ tmp_int_i;

        // find closest mapping
        closest_core = ref_core;
        for(j = 0; j<comm_size; j++){ 
            if(mapping[j]>=0) continue;
            if(topo_info[comm_size*ref_core+j] > topo_info[comm_size*ref_core+closest_core])
                closest_core = j;
        }

        mapping[closest_core] = new_rank;
        num_rank_mappings++;

        if (num_rank_mappings == 2){
            ref_rank = new_rank;
            ref_core = closest_core;
            tmp_int_i = 1;
            num_rank_mappings = 0;
        }
    }

    if(mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm)){
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce RABEN coll_remap_create_new_cached_comm failed"));
        free(mapping);
        return OMPI_ERROR;
    }
    
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce RABEN rank %d on core %d", ompi_comm_rank(*new_comm), rank));
    free(mapping);
    return OMPI_SUCCESS;
}

/*
    This should map processes cyclicly accross the different topo values  
    it should lower congestion
*/
int remap_allreduce_linear_remap(ompi_communicator_t *old_comm,
                                  mca_coll_remap_module_t *module,
                                  ompi_communicator_t **new_comm)
{ 
    int comm_size, rank, i, j, j_distance;
    int ref_rank, prev_distance, nxt_distance, nxt_core; 
    int *topo_info=module->proc_locality_arr, *mapping;
    rank = ompi_comm_rank(old_comm);
    comm_size = ompi_comm_size(old_comm);

    mapping = malloc(comm_size*sizeof(int));
    for(i = 1; i<comm_size; i++)mapping[i] = -1;
    mapping[0] = 0; 
    ref_rank=0;
    prev_distance = INT_MAX;

    for(i=1; i<comm_size; i++){
        // find next rank to map
        nxt_core = -1;
        nxt_distance = -1;

        // find closest mapping furthur then previous mapping
        for(j = 0; j<comm_size; j++){ 
            if(mapping[j]>=0) continue;
            j_distance = topo_info[comm_size*ref_rank+j];
            if(j_distance<prev_distance && j_distance > nxt_distance){
                nxt_core = j;
                nxt_distance = j_distance;
            }
        }
        
        if (nxt_core == -1){ // couldn't find a next core to map
            for(j = 0; j<comm_size; j++){ 
                if(mapping[j]>=0) continue;
                j_distance = topo_info[comm_size*ref_rank+j];
                if(j_distance>nxt_distance){
                    nxt_core = j;
                    nxt_distance = j_distance;
                }
            }
        }

        mapping[nxt_core] = i;
        prev_distance = nxt_distance;

    }

    if(mca_coll_remap_create_new_cached_comm(old_comm, mapping, new_comm)){
        OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce LINEAR coll_remap_create_new_cached_comm failed"));
        free(mapping);
        return OMPI_ERROR;
    }
    
    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:allreduce LINEAR rank %d on core %d", ompi_comm_rank(*new_comm), rank));
    free(mapping);
    return OMPI_SUCCESS;
}


/** Algorithms:
 *  {1, "basic_linear"},
 *  {2, "nonoverlapping"},
 *  {3, "recursive_doubling"},
 *  {4, "ring"},
 *  {5, "segmented_ring"},
 *  {6, "rabenseifner"
 *
 * Currently, ring, segmented ring, and rabenseifner do not support
 * non-commutative operations.
 */
int remap_allreduce_pick_alg(int count, struct ompi_datatype_t *dtype, 
                            struct ompi_communicator_t *comm)
{
    int communicator_size, alg;
    size_t total_dsize, dsize;

    communicator_size = ompi_comm_size(comm);
    ompi_datatype_type_size(dtype, &dsize);
    total_dsize = dsize * (ptrdiff_t)count;

    if (communicator_size < 4) {
        if (total_dsize < 8) {
            alg = 4;
        } else if (total_dsize < 4096) {
            alg = 3;
        } else if (total_dsize < 8192) {
            alg = 4;
        } else if (total_dsize < 16384) {
            alg = 3;
        } else if (total_dsize < 65536) {
            alg = 4;
        } else if (total_dsize < 262144) {
            alg = 5;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 8) {
        if (total_dsize < 16) {
            alg = 4;
        } else if (total_dsize < 8192) {
            alg = 3;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 16) {
        if (total_dsize < 8192) {
            alg = 3;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 32) {
        if (total_dsize < 64) {
            alg = 5;
        } else if (total_dsize < 4096) {
            alg = 3;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 64) {
        if (total_dsize < 128) {
            alg = 5;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 128) {
        if (total_dsize < 128) {
            alg = 1;
        } else if (total_dsize < 512) {
            alg = 3;
        } else if (total_dsize < 8192) {
            alg = 1;
        } else if (total_dsize < 262144) {
            alg = 3;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 256) {
        if (total_dsize < 2048) {
            alg = 2;
        } else if (total_dsize < 16384) {
            alg = 1;
        } else if (total_dsize < 131072) {
            alg = 2;
        } else if (total_dsize < 262144) {
            alg = 3;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 512) {
        if (total_dsize < 4096) {
            alg = 2;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 2048) {
        if (total_dsize < 2048) {
            alg = 2;
        } else if (total_dsize < 16384) {
            alg = 3;
        } else {
            alg = 6;
        }
    }
    else if (communicator_size < 4096) {
        if (total_dsize < 2048) {
            alg = 2;
        } else if (total_dsize < 4096) {
            alg = 5;
        } else if (total_dsize < 16384) {
            alg = 3;
        } else {
            alg = 6;
        }
    }
    else {
        if (total_dsize < 2048) {
            alg = 2;
        } else if (total_dsize < 16384) {
            alg = 5;
        } else if (total_dsize < 32768) {
            alg = 3;
        } else {
            alg = 6;
        }
    }

    return alg;
}
