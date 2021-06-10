#ifndef MCA_COLL_REMAP_SCOTCH_H_HAS_BEEN_EXPORTED
#define MCA_COLL_REMAP_SCOTCH_H_HAS_BEEN_EXPORTED

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "scotch.h"
#include "ompi_config.h"
#include "coll_remap.h"

BEGIN_C_DECLS

typedef struct mca_coll_remap_scotch_graph_data
{
    SCOTCH_Num *velotab;
    SCOTCH_Num *verttab;
    SCOTCH_Num *vendtab;
    SCOTCH_Num *vlbltab;
    SCOTCH_Num *edgetab;
    SCOTCH_Num *edlotab;
} mca_coll_remap_scotch_graph_data;

void free_graphArrData(mca_coll_remap_scotch_graph_data *g_d);

int mca_coll_remap_scotch_build_ring_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d);
int mca_coll_remap_scotch_build_rdouble_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d);
int mca_coll_remap_scotch_build_rsa_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d);
int mca_coll_remap_scotch_build_bintree_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d);
int mca_coll_remap_scotch_build_knomial_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d, int k);
int mca_coll_remap_scotch_build_scag_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d);

int mca_coll_remap_scotch_build_topo_arch(SCOTCH_Arch *a, int *topo_mat, int comm_size);

END_C_DECLS
#endif // MCA_COLL_REMAP_SCOTCH_H_HAS_BEEN_EXPORTED
