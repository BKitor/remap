#include "coll_remap.h"

#include "opal/util/show_help.h"
#include "opal/util/output.h"

#include "ompi/communicator/communicator.h"
#include "ompi/mca/coll/base/base.h"
#include "ompi/mca/coll/base/coll_base_functions.h"

static void mca_coll_remap_module_construct(mca_coll_remap_module_t *module){
    module->proc_locality_arr = NULL;

    module->cached_allreduce_comm = NULL;
    module->cached_bcast_comm = NULL;

    module->cached_base_data = NULL;

    module->fallback_allreduce_fn = NULL;
    module->fallback_allreduce_module = NULL;
    module->fallback_bcast_fn = NULL;
    module->fallback_bcast_module = NULL;
}

static void mca_coll_remap_module_destruct(mca_coll_remap_module_t *module){
    int i;

    /* free proc locality info */
    free(module->proc_locality_arr);
    module->proc_locality_arr = NULL;

    /* release cached communicators */
    for(i = 0; i < REMAP_ALLREDUCE_ALG_COUNT; i++){
        if(NULL != module->cached_allreduce_comm[i]){
            ompi_comm_free(&(module->cached_allreduce_comm[i]));
            module->cached_allreduce_comm[i] = NULL;
        }
    }
    free(module->cached_allreduce_comm);

    for(i = 0; i < REMAP_BCAST_ALG_COUNT; i++){
        if(NULL != module->cached_bcast_comm[i]){
            ompi_comm_free(&(module->cached_bcast_comm[i]));
            module->cached_bcast_comm[i] = NULL;
        }
    }
    free(module->cached_bcast_comm);

    // release remapped_base_data
    OBJ_RELEASE(module->cached_base_data);

    /* release fallback modules & functions */
    module->fallback_allreduce_fn = NULL;
    if(NULL != module->fallback_allreduce_module){
        OBJ_RELEASE(module->fallback_allreduce_module);
    }
    module->fallback_allreduce_module = NULL;

    module->fallback_bcast_fn = NULL;
    if(NULL != module->fallback_bcast_module){
        OBJ_RELEASE(module->fallback_bcast_module);
    }
    module->fallback_bcast_module = NULL;

}
OBJ_CLASS_INSTANCE(mca_coll_remap_module_t, mca_coll_base_module_t,
                   mca_coll_remap_module_construct,
                   mca_coll_remap_module_destruct);

int mca_coll_remap_init_query(bool enable_progress_threads, 
                                      bool enable_mpi_threads){
    return OMPI_SUCCESS;
}

mca_coll_base_module_t *
mca_coll_remap_comm_query(struct ompi_communicator_t *comm,
                                int *priority){
    mca_coll_remap_module_t* remap_module;

    if(OMPI_COMM_IS_INTER(comm)){
        /* can only use intra comms */
        *priority = 0;
        return NULL;
    }

    if(ompi_comm_size(comm)<2){
        /* make sure there are at least 2 ranks in comm */
        *priority = 0;
        return NULL;
    }

    *priority = mca_coll_remap_component.priority;
    if(mca_coll_remap_component.priority <= 0){
        /* if priority <=0 comm is unavalable,
        most likely to happen when creating a cahced comm */
        return NULL;
    }

    remap_module = OBJ_NEW(mca_coll_remap_module_t);

    remap_module->super.coll_module_enable = mca_coll_remap_module_enable;
    remap_module->super.ft_event = NULL;

    remap_module->super.coll_allreduce = mca_coll_remap_allreduce_intra;
    remap_module->super.coll_bcast = mca_coll_remap_bcast_intra;

    // remap_module->super.coll_allgather = NULL;
    // remap_module->super.coll_gather = NULL;
    // remap_module->super.coll_reduce = NULL;

    return &(remap_module->super);
}


int mca_coll_remap_module_enable(mca_coll_base_module_t *module,
                                    struct ompi_communicator_t *comm){
    mca_coll_remap_module_t *m = (mca_coll_remap_module_t*) module;
    mca_coll_base_comm_t *data = NULL, *remapped_data = NULL;

    /* setup fallback allreduce fn & module */
    if(NULL == comm->c_coll->coll_allreduce_module){
        opal_output_verbose(1, ompi_coll_base_framework.framework_output, 
            "(%d, %s) No underlying allreduce api, disqualifying myself",
            comm->c_contextid, comm->c_name );
        return OMPI_ERROR;
    }

    if(NULL == comm->c_coll->coll_bcast_module){
        opal_output_verbose(1, ompi_coll_base_framework.framework_output, 
            "(%d, %s) No underlying bcast api, disqualifying myself",
            comm->c_contextid, comm->c_name );
        return OMPI_ERROR;
    }

    m->fallback_allreduce_module = comm->c_coll->coll_allreduce_module;
    m->fallback_allreduce_fn = comm->c_coll->coll_allreduce;
    OBJ_RETAIN(comm->c_coll->coll_allreduce_module);

    m->fallback_bcast_module = comm->c_coll->coll_bcast_module;
    m->fallback_bcast_fn = comm->c_coll->coll_bcast;
    OBJ_RETAIN(comm->c_coll->coll_bcast_module);

    /* allocate pointers for cached comms 
            might wan to revamp and make easer to extend to more colls
    */
    m->cached_allreduce_comm = (ompi_communicator_t**) calloc(REMAP_ALLREDUCE_ALG_COUNT, sizeof(ompi_communicator_t*));
    if(NULL == m->cached_allreduce_comm){
        opal_output_verbose(1, ompi_coll_base_framework.framework_output, 
            "Couldn't calloc cached_allredcue_comm pointers, exiting");
        return OMPI_ERROR;
    }

    m->cached_bcast_comm = (ompi_communicator_t**) calloc(REMAP_BCAST_ALG_COUNT, sizeof(ompi_communicator_t*));
    if(NULL == m->cached_bcast_comm){
        opal_output_verbose(1, ompi_coll_base_framework.framework_output, 
            "Couldn't calloc cached_bcast_comm pointers, exiting");
        return OMPI_ERROR;
    }

    data = OBJ_NEW(mca_coll_base_comm_t);
    if(NULL == data){
        opal_output_verbose(1, ompi_coll_base_framework.framework_output,
                            "coll_remap ran out of memory allocating mca_coll_base_comm_t");
        return OMPI_ERROR;
    }
    /* general n fan out tree */
    data->cached_ntree = NULL;
    /* binary tree */
    data->cached_bintree = NULL;
    /* binomial tree */
    data->cached_bmtree = NULL;
    /* binomial tree */
    data->cached_in_order_bmtree = NULL;
    /* k-nomial tree */
    data->cached_kmtree = NULL;
    /* chains (fanout followed by pipelines) */
    data->cached_chain = NULL;
    /* standard pipeline */
    data->cached_pipeline = NULL;
    /* in-order binary tree */
    data->cached_in_order_bintree = NULL;

    m->super.base_data = data;


    remapped_data = OBJ_NEW(mca_coll_base_comm_t);

    if(NULL == remapped_data){
        opal_output_verbose(1, ompi_coll_base_framework.framework_output,
                            "coll_remap ran out of memory allocating mca_coll_base_comm_t");
        return OMPI_ERROR;
    }
    remapped_data->cached_ntree = NULL;
    remapped_data->cached_bintree = NULL;
    remapped_data->cached_bmtree = NULL;
    remapped_data->cached_in_order_bmtree = NULL;
    remapped_data->cached_kmtree = NULL;
    remapped_data->cached_chain = NULL;
    remapped_data->cached_pipeline = NULL;
    remapped_data->cached_in_order_bintree = NULL;

    m->cached_base_data = remapped_data;

    OPAL_OUTPUT((ompi_coll_remap_stream, "coll:remap:module_enable exited sussesfully"));
    return OMPI_SUCCESS;
}
