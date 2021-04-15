#ifndef MCA_COLL_REMAP_EXPORT_H
#define MCA_COLL_REMAP_EXPORT_H


#include "ompi_config.h"

#include "mpi.h"
#include "ompi/mca/mca.h"
#include "ompi/mca/coll/coll.h"

BEGIN_C_DECLS

extern int ompi_coll_remap_stream;

int mca_coll_remap_init_query(bool enable_progress_threads,
                                bool enable_mpi_threads);
mca_coll_base_module_t
    *mca_coll_remap_comm_query(struct ompi_communicator_t *comm,
                                int *priority);

int mca_coll_remap_module_enable(mca_coll_base_module_t *module,
                                    struct ompi_communicator_t *comm);

int mca_coll_remap_allreduce_intra(const void *sbuf, void *rbuf, int count,
                                       struct ompi_datatype_t *dtype,
                                       struct ompi_op_t *op,
                                       struct ompi_communicator_t *comm,
                                       mca_coll_base_module_t *module);

int mca_coll_remap_bcast_intra(void *buf, int count, struct ompi_datatype_t *dtype, int root,
                               struct ompi_communicator_t *comm, mca_coll_base_module_t *module);

typedef struct mca_coll_remap_module_t{
    mca_coll_base_module_t super;

    int *proc_locality_arr;

    // pointers to cached communicators
    struct ompi_communicator_t **cached_allreduce_comm;
    struct ompi_communicator_t **cached_bcast_comm;

    struct mca_coll_base_comm_t *cached_base_data;

    // fallback allreduce alg
    mca_coll_base_module_allreduce_fn_t fallback_allreduce_fn;
    mca_coll_base_module_t *fallback_allreduce_module;
    mca_coll_base_module_bcast_fn_t fallback_bcast_fn;
    mca_coll_base_module_t *fallback_bcast_module;

} mca_coll_remap_module_t;

OBJ_CLASS_DECLARATION(mca_coll_remap_module_t);

typedef struct mca_coll_remap_component_t{
    mca_coll_base_component_2_0_0_t super;
    int priority;
    int select_allreduce_alg;
    int select_bcast_alg;
    int turn_off_remap;
    int cc_cluster;
    char* net_topo_input_mat;
} mca_coll_remap_component_t;

OMPI_MODULE_DECLSPEC extern mca_coll_remap_component_t mca_coll_remap_component;

/* This function fills module->proc_locality_arr */
int mca_coll_remap_set_proc_locality_info(struct ompi_communicator_t *comm,
                                            mca_coll_remap_module_t *module);

int mca_coll_remap_create_new_cached_comm(struct ompi_communicator_t *old_comm, int* mapping,
                                          struct ompi_communicator_t **new_comm);

// stuff for remap_allreduce
enum MCA_COLL_REMAP_ALLREDUCE_ALG{
    REMAP_ALLREDUCE_ALG_IGNORE = 0,
    REMAP_ALLREDUCE_ALG_LINEAR,
    REMAP_ALLREDUCE_ALG_NON_OVERLAP,
    REMAP_ALLREDUCE_ALG_RECURSIVE_DOUBLING,
    REMAP_ALLREDUCE_ALG_RING,
    REMAP_ALLREDUCE_ALG_SEGMENTED_RING,
    REMAP_ALLREDUCE_ALG_RABENSEIFNER,
    REMAP_ALLREDUCE_ALG_COUNT
};

int remap_allreduce_pick_alg(int count, struct ompi_datatype_t *dtype, 
                            struct ompi_communicator_t *comm);

int remap_allreduce_ring_remap(struct ompi_communicator_t *old_comm,
                               mca_coll_remap_module_t *module,
                               struct ompi_communicator_t **new_comm);

int remap_allreduce_rdouble_remap(struct ompi_communicator_t *old_comm,
                                  mca_coll_remap_module_t *module,
                                  struct ompi_communicator_t **new_comm);

int remap_allreduce_raben_remap(struct ompi_communicator_t *old_comm,
                                  mca_coll_remap_module_t *module,
                                  struct ompi_communicator_t **new_comm);

int remap_allreduce_linear_remap(struct ompi_communicator_t *old_comm,
                                  mca_coll_remap_module_t *module,
                                  struct ompi_communicator_t **new_comm);

// stuff for remap_bcast
enum MCA_COLL_REMAP_BCAST_ALG{
    REMAP_BCAST_ALG_IGNORE = 0,
    REMAP_BCAST_ALG_LINEAR,
    REMAP_BCAST_ALG_CHAIN,
    REMAP_BCAST_ALG_PIPELINE,
    REMAP_BCAST_ALG_SPLIT_BIN_TREE,
    REMAP_BCAST_ALG_BIN_TREE,
    REMAP_BCAST_ALG_BINOMIAL,
    REMAP_BCAST_ALG_KNOMIAL,
    REMAP_BCAST_ALG_SCATTER_ALLGATHER,
    REMAP_BCAST_ALG_SCATTER_ALLGATHER_RING,
    REMAP_BCAST_ALG_COUNT
};

enum REMAP_CC_CLUSTERS{
    CC_NULL=0,
    CC_NIAGARA,
    CC_MIST,
    CC_CEDAR,
    CC_GRAHAM,
    CC_BELUGA,
    CC_CLUSTER_COUNT
};

int remap_bcast_pick_alg(int count, struct ompi_datatype_t *datatype,  
                         struct ompi_communicator_t *comm);

int remap_bcast_pipeline_remap(struct ompi_communicator_t *old_comm,
                               mca_coll_remap_module_t *module,
                               struct ompi_communicator_t **new_comm);

int remap_bcast_binomial_remap(struct ompi_communicator_t *old_comm,
                               mca_coll_remap_module_t *module,
                               struct ompi_communicator_t **new_comm);

int remap_bcast_knomial_remap(struct ompi_communicator_t *old_comm,
                               mca_coll_remap_module_t *module,
                               struct ompi_communicator_t **new_comm, int radix);

int remap_bcast_scatter_allgather_remap(struct ompi_communicator_t *old_comm,
                              mca_coll_remap_module_t *module,
                              struct ompi_communicator_t **new_comm);

int remap_bcast_bintree_remap(struct ompi_communicator_t *old_comm,
                              mca_coll_remap_module_t *module,
                              struct ompi_communicator_t **new_comm);

END_C_DECLS

#endif //MCA_COLL_REMAP_EXPORT_H 