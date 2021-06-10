#include "coll_remap_scotch.h"

static int inline _log_2(int n)
{
    int lvl = 0;
    while (n >>= 1)
        lvl++;
    return lvl;
}

static int inline _log_k(int n, int k)
{
    int lvl = 0;
    while (n /= k)
        lvl++;
    return lvl;
}

// there's a beter way to do this, see 'exponentiation by squaring'
static int inline _k_pow_n(int k, int n)
{
    int res = 1;
    for (int i = 0; i < n; i++)
        res *= k;
    return res;
}

// greatest divisible power of k
// for example(k=4): 16 => 16, 17 =>0, 20 => 4, 32=>16, 64=>64
static int inline _gdp_k(int n, int k)
{
    if (n % k)
        return 0;
    if (n == 0)
        return 0;

    int p;

    for (p = k; n % p == 0; p *= k)
        ;
    p /= k;

    return p;
}

void free_graphArrData(mca_coll_remap_scotch_graph_data *g_d)
{
    free(g_d->velotab);
    free(g_d->verttab);
    free(g_d->vendtab);
    free(g_d->vlbltab);
    free(g_d->edgetab);
    free(g_d->edlotab);
}

// for(int i = 0; i<=128; i++) _gdp_k(i, 4);
// for(int j = 2; j<6; j++)
// for(int i = j; i<512; i*=j)printf("%d %d %d\n", j, i, _log_k(i, j));

int mca_coll_remap_scotch_build_ring_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d)
{
    SCOTCH_Num baseval = 0;
    SCOTCH_Num vertnbr = (SCOTCH_Num)comm_size;
    g_d->velotab = NULL;
    g_d->verttab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vendtab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vlbltab = malloc(vertnbr * sizeof(SCOTCH_Num));
    SCOTCH_Num edgenbr = 2 * vertnbr;
    g_d->edgetab = malloc(edgenbr * sizeof(SCOTCH_Num));
    g_d->edlotab = malloc(edgenbr * sizeof(SCOTCH_Num));

    for (int i = 0; i < vertnbr; i++)
    {
        g_d->verttab[i] = i * 2;
        g_d->vendtab[i] = i * 2 + 2;
        g_d->edgetab[i * 2] = (i - 1 + comm_size) % comm_size;
        g_d->edgetab[i * 2 + 1] = (i + 1 + comm_size) % comm_size;
        g_d->vlbltab[i] = i;
    }
    for (int i = 0; i < edgenbr; i++)
    {
        g_d->edlotab[i] = 1;
    }

    if (0 != SCOTCH_graphBuild(g, baseval, vertnbr,
                               g_d->verttab, g_d->vendtab,
                               g_d->velotab, g_d->vlbltab,
                               edgenbr, g_d->edgetab, g_d->edlotab))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_ring_comm_graph: ERROR, SCOTCH_graphBuild Failed, aborting"));
        return OMPI_ERROR;
    }
    if (0 != SCOTCH_graphCheck(g))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_ring_comm_graph: ERROR, SCOTCH_graphCheck Failed, aborting"));
        return OMPI_ERROR;
    }
    OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_ring_comm_graph: ring graph built"));

    return OMPI_SUCCESS;
}

// only works if comm_size is a power of 2
int mca_coll_remap_scotch_build_rdouble_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d)
{
    int num_rounds = _log_2(comm_size);

    SCOTCH_Num baseval = 0;
    SCOTCH_Num vertnbr = (SCOTCH_Num)comm_size;
    g_d->velotab = NULL;
    g_d->verttab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vendtab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vlbltab = malloc(vertnbr * sizeof(SCOTCH_Num));
    SCOTCH_Num edgenbr = (SCOTCH_Num)num_rounds * vertnbr;
    g_d->edgetab = malloc(edgenbr * sizeof(SCOTCH_Num));
    g_d->edlotab = malloc(edgenbr * sizeof(SCOTCH_Num));

    for (int i = 0; i < vertnbr; i++)
    {
        g_d->verttab[i] = i * num_rounds;
        g_d->vendtab[i] = (i + 1) * num_rounds;
        g_d->vlbltab[i] = i;
        for (int itr = 0, j = 1; j < comm_size; j <<= 1, itr++)
        {
            g_d->edgetab[i * num_rounds + itr] = i ^ j;
        }
    }
    for (int i = 0; i < edgenbr; i++)
    {
        g_d->edlotab[i] = 1;
    }

    if (0 != SCOTCH_graphBuild(g, baseval, vertnbr,
                               g_d->verttab, g_d->vendtab,
                               g_d->velotab, g_d->vlbltab,
                               edgenbr, g_d->edgetab, g_d->edlotab))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_rdouble_comm_graph: ERROR, SCOTCH_graphBuild Failed, aborting"));
        return OMPI_ERROR;
    }
    if (0 != SCOTCH_graphCheck(g))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_rdouble_comm_graph: ERROR, SCOTCH_graphCheck Failed, aborting"));
        return OMPI_ERROR;
    }
    OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_rdouble_comm_graph: rsa graph built"));

    return OMPI_SUCCESS;
}

// only works if comm_size is a power of 2
int mca_coll_remap_scotch_build_rsa_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d)
{
    int num_rounds = _log_2(comm_size);

    SCOTCH_Num baseval = 0;
    SCOTCH_Num vertnbr = (SCOTCH_Num)comm_size;
    g_d->velotab = NULL;
    g_d->verttab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vendtab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vlbltab = malloc(vertnbr * sizeof(SCOTCH_Num));
    SCOTCH_Num edgenbr = (SCOTCH_Num)num_rounds * vertnbr;
    g_d->edgetab = malloc(edgenbr * sizeof(SCOTCH_Num));
    g_d->edlotab = malloc(edgenbr * sizeof(SCOTCH_Num));

    for (int i = 0; i < vertnbr; i++)
    {
        g_d->verttab[i] = i * num_rounds;
        g_d->vendtab[i] = (i + 1) * num_rounds;
        g_d->vlbltab[i] = i;
        for (int itr = 0, j = 1; j < comm_size; j <<= 1, itr++)
        {
            g_d->edgetab[i * num_rounds + itr] = i ^ j;
            g_d->edlotab[i * num_rounds + itr] = comm_size / (2 * j);
        }
    }

    if (0 != SCOTCH_graphBuild(g, baseval, vertnbr,
                               g_d->verttab, g_d->vendtab,
                               g_d->velotab, g_d->vlbltab,
                               edgenbr, g_d->edgetab, g_d->edlotab))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_rsa_comm_graph: ERROR, SCOTCH_graphBuild Failed, aborting"));
        return OMPI_ERROR;
    }
    if (0 != SCOTCH_graphCheck(g))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_rsa_comm_graph: ERROR, SCOTCH_graphCheck Failed, aborting"));
        return OMPI_ERROR;
    }

    OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_rsa_comm_graph: rsa graph built"));

    return OMPI_SUCCESS;
}

// careful with how small you go, not gaurenteed to work when comm_size < 4
int mca_coll_remap_scotch_build_bintree_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d)
{
    SCOTCH_Num baseval = 0;
    SCOTCH_Num vertnbr = (SCOTCH_Num)comm_size;
    g_d->velotab = NULL;
    g_d->verttab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vendtab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vlbltab = malloc(vertnbr * sizeof(SCOTCH_Num));
    SCOTCH_Num edgenbr = (SCOTCH_Num)(comm_size - 1) * 2;
    g_d->edgetab = malloc(edgenbr * sizeof(SCOTCH_Num));
    g_d->edlotab = malloc(edgenbr * sizeof(SCOTCH_Num));

    g_d->vlbltab[0] = 0;
    g_d->verttab[0] = 0;
    g_d->vendtab[0] = 2;
    g_d->edgetab[0] = 1;
    g_d->edgetab[1] = 2;

    for (int i = 1; i < vertnbr; i++)
    {
        g_d->vlbltab[i] = i;
        int lvl = _log_2(i + 1);
        int c1 = i + (1 << lvl);
        int c2 = i + (1 << (lvl + 1));
        int num_edges = 1;
        if (c1 < comm_size)
            num_edges++;
        if (c2 < comm_size)
            num_edges++;

        g_d->verttab[i] = g_d->vendtab[i - 1];
        g_d->vendtab[i] = g_d->verttab[i] + num_edges;
    }

    g_d->edgetab[0] = 1;
    g_d->edgetab[1] = 2;
    g_d->edgetab[2] = 0;
    g_d->edgetab[5] = 0;
    for (int i = 1; i < vertnbr; i++)
    {
        int lvl = _log_2(i + 1);
        int c1 = i + (1 << lvl);
        int c2 = i + (1 << (lvl + 1));

        if (c1 < comm_size)
        {
            g_d->edgetab[g_d->verttab[i] + 1] = c1;
            g_d->edgetab[g_d->verttab[c1]] = i;
        }
        if (c2 < comm_size)
        {
            g_d->edgetab[g_d->verttab[i] + 2] = c2;
            g_d->edgetab[g_d->verttab[c2]] = i;
        }
    }

    for (int i = 0; i < edgenbr; i++)
    {
        g_d->edlotab[i] = 1;
    }

    if (0 != SCOTCH_graphBuild(g, baseval, vertnbr,
                               g_d->verttab, g_d->vendtab,
                               g_d->velotab, g_d->vlbltab,
                               edgenbr, g_d->edgetab, g_d->edlotab))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_bintree_comm_graph: ERROR, SCOTCH_graphBuild Failed, aborting"));
        return OMPI_ERROR;
    }
    if (0 != SCOTCH_graphCheck(g))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_bintree_comm_graph: ERROR, SCOTCH_graphCheck Failed, aborting"));
        return OMPI_ERROR;
    }

    OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_bintree_comm_graph: bin_tree graph built"));

    return OMPI_SUCCESS;
}

// assumig comm_size is always a power of 2, and k is 4
int mca_coll_remap_scotch_build_knomial_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d, int k)
{
    SCOTCH_Num baseval = 0;
    SCOTCH_Num vertnbr = (SCOTCH_Num)comm_size;
    g_d->velotab = NULL;
    g_d->verttab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vendtab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vlbltab = malloc(vertnbr * sizeof(SCOTCH_Num));
    SCOTCH_Num edgenbr = (SCOTCH_Num)(comm_size - 1) * 2;
    g_d->edgetab = malloc(edgenbr * sizeof(SCOTCH_Num));
    g_d->edlotab = malloc(edgenbr * sizeof(SCOTCH_Num));

    for (int i = 0; i < vertnbr; i++)
    {
        g_d->vlbltab[i] = i;
    }
    for (int i = 0; i < edgenbr; i++)
    {
        g_d->edlotab[i] = 1;
    }

    // num_children = subtree depth * (k-1)
    int gdp_k = _gdp_k(comm_size, k);
    int subtree_depth = _log_k(gdp_k, k);

    // map all of 0's children
    g_d->verttab[0] = 0;
    g_d->vendtab[0] = 0;
    for (int i = 0; i <= subtree_depth; i++)
    {
        int child_tmp = _k_pow_n(k, i);
        for (int j = 0; j < (k - 1); j++)
        {
            if (child_tmp * (j + 1) < comm_size)
            {
                g_d->edgetab[i * (k - 1) + j] = child_tmp * (j + 1);
                g_d->vendtab[0]++;
            }
        }
    }

    // fill out verttab and vendtab
    for (int i = 1; i < comm_size; i++)
    {
        int subtree_depth = _log_k(_gdp_k(i, k), k);
        int num_children = subtree_depth * (k - 1);
        g_d->verttab[i] = g_d->vendtab[i - 1];
        g_d->vendtab[i] = g_d->verttab[i] + num_children + 1;
    }

    // fill out edgetab
    void _rec_knom_fn(int ref_rank)
    {
        int rr = (ref_rank) ? ref_rank : comm_size;
        int subtree_depth = _log_k(_gdp_k(rr, k), k);

        if (ref_rank == 0)
            subtree_depth += 1;

        int num_children = subtree_depth * (k - 1);

        for (int i = 0; i < subtree_depth; i++)
        {
            int tmp_pow_n = _k_pow_n(k, i);
            for (int j = 1; j < k; j++)
            {
                int child = ref_rank + j * tmp_pow_n;
                if (ref_rank == 0 && child >= comm_size)
                    break;
                // map child rank to parent
                g_d->edgetab[g_d->verttab[child]] = ref_rank;
                if (ref_rank != 0)
                {
                    g_d->edgetab[g_d->verttab[ref_rank] + (i * (k - 1) + j)] = child;
                }
                _rec_knom_fn(child);
            }
        }
    }
    _rec_knom_fn(0);

    if (0 != SCOTCH_graphBuild(g, baseval, vertnbr,
                               g_d->verttab, g_d->vendtab,
                               g_d->velotab, g_d->vlbltab,
                               edgenbr, g_d->edgetab, g_d->edlotab))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_knom_comm_graph: ERROR, SCOTCH_graphBuild Failed, aborting"));
        return OMPI_ERROR;
    }
    if (0 != SCOTCH_graphCheck(g))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_knom_comm_graph: ERROR, SCOTCH_graphCheck Failed, aborting"));
        return OMPI_ERROR;
    }
    OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_knom_comm_graph: knom graph built"));

    return OMPI_SUCCESS;
}

// only works if comm_size is a power of 2
int mca_coll_remap_scotch_build_scag_comm_graph(SCOTCH_Graph *g, int comm_size, mca_coll_remap_scotch_graph_data *g_d)
{
    int num_rounds = _log_2(comm_size);

    SCOTCH_Num baseval = 0;
    SCOTCH_Num vertnbr = (SCOTCH_Num)comm_size;
    g_d->velotab = NULL;
    g_d->verttab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vendtab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d->vlbltab = malloc(vertnbr * sizeof(SCOTCH_Num));
    SCOTCH_Num edgenbr = (SCOTCH_Num)num_rounds * vertnbr;
    g_d->edgetab = malloc(edgenbr * sizeof(SCOTCH_Num));
    g_d->edlotab = malloc(edgenbr * sizeof(SCOTCH_Num));

    //recursive doubling pattern for the all-gather stage
    for (int i = 0; i < vertnbr; i++)
    {
        g_d->verttab[i] = i * num_rounds;
        g_d->vendtab[i] = (i + 1) * num_rounds;
        g_d->vlbltab[i] = i;
        for (int itr = 0, j = 1; j < comm_size; j <<= 1, itr++)
        {
            g_d->edgetab[i * num_rounds + itr] = i ^ j;
            g_d->edlotab[i * num_rounds + itr] = j;
        }
    }

    // recursive funciton step for the binomial gather phase
    void _rec_fn_scag(int ref_rank, int dist)
    {
        for (int i = _log_2(dist), d = dist; i >= 0 && d != 0; i--, d /= 2)
        {
            // edge from ref_rank to ref_rank^w
            g_d->edlotab[ref_rank * num_rounds + i] += d;
            g_d->edlotab[(ref_rank ^ d) * num_rounds + i] += d;
            _rec_fn_scag(ref_rank ^ d, d / 2);
        }
    }
    _rec_fn_scag(0, comm_size / 2);

    if (0 != SCOTCH_graphBuild(g, baseval, vertnbr,
                               g_d->verttab, g_d->vendtab,
                               g_d->velotab, g_d->vlbltab,
                               edgenbr, g_d->edgetab, g_d->edlotab))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_scag_comm_graph: ERROR, SCOTCH_graphBuild Failed, aborting"));
        return OMPI_ERROR;
    }
    if (0 != SCOTCH_graphCheck(g))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_scag_comm_graph: ERROR, SCOTCH_graphCheck Failed, aborting"));
        return OMPI_ERROR;
    }
   OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_scag_comm_graph: scag graph built"));

    return OMPI_SUCCESS;
}

int mca_coll_remap_scotch_build_topo_arch(SCOTCH_Arch *a, int *topo_mat, int comm_size)
{
    SCOTCH_Graph *g = SCOTCH_graphAlloc();
    SCOTCH_graphInit(g);
    SCOTCH_Strat *s = SCOTCH_stratAlloc();
    SCOTCH_stratInit(s);
    mca_coll_remap_scotch_graph_data g_d;

    SCOTCH_Num baseval = 0;
    SCOTCH_Num vertnbr = (SCOTCH_Num)comm_size;
    g_d.velotab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d.verttab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d.vendtab = malloc(vertnbr * sizeof(SCOTCH_Num));
    g_d.vlbltab = malloc(vertnbr * sizeof(SCOTCH_Num));
    SCOTCH_Num edgenbr = (SCOTCH_Num)(vertnbr - 1) * (vertnbr);
    g_d.edgetab = malloc(edgenbr * sizeof(SCOTCH_Num));
    g_d.edlotab = malloc(edgenbr * sizeof(SCOTCH_Num));

    // printf("edgenum:%d\n", edgenbr);

    for (int i = 0; i < vertnbr; i++)
    {
        g_d.vlbltab[i] = i;
        g_d.velotab[i] = 3; // greater than 1 to provide a better mapping
        g_d.verttab[i] = (i == 0) ? 0 : g_d.vendtab[i - 1];
        g_d.vendtab[i] = g_d.verttab[i] + vertnbr - 1;
        for (int j = 0; j < comm_size; j++)
        {
            if (i == j)
                continue;

            int cur_idx = g_d.verttab[i] + j;
            cur_idx -= (i < j) ? 1 : 0;
            g_d.edgetab[cur_idx] = j;
            g_d.edlotab[cur_idx] = topo_mat[i * comm_size + j] + 1;

            // printf("(i, j):(%d, %d)->%d, topo_map:%d\n", i, j, cur_idx, g_d.edlotab[cur_idx]);
        }
        // printf("%d, %d\n", g_d.verttab[i], g_d.vendtab[i]);
    }

    if (0 != SCOTCH_graphBuild(g, baseval, vertnbr,
                               g_d.verttab, g_d.vendtab,
                               g_d.velotab, g_d.vlbltab,
                               edgenbr, g_d.edgetab, g_d.edlotab))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_topo_arch: ERROR, SCOTCH_graphBuild Failed, aborting"));
        return OMPI_ERROR;
    }
    if (0 != SCOTCH_graphCheck(g))
    {
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_topo_arch: ERROR, SCOTCH_graphCheck Failed, aborting"));
        return OMPI_ERROR;
    }

    if (0 != SCOTCH_archBuild(a, g, 0, NULL, s))
    {
        // printf("build_topo_arch: graph build failed\n");
        OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_topo_arch: ERROR, SCOTCH_archBuild Failed, aborting"));
        return OMPI_ERROR;
    }

    SCOTCH_stratExit(s);
    SCOTCH_graphExit(g);
    free_graphArrData(&g_d);
    SCOTCH_memFree(g);
    SCOTCH_memFree(s);

    OPAL_OUTPUT((ompi_coll_remap_stream, "scotch_build_topo_arch_comm_graph: topo arch graph built"));

    return OMPI_SUCCESS;
}
