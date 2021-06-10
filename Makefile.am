sources = \
        coll_remap.h \
        coll_remap_component.c \
        coll_remap_module.c \
        coll_remap_allreduce.c \
        coll_remap_bcast.c \
        coll_remap_topo_info.c \
        coll_remap_scotch.h \
        coll_remap_scotch.c 


if MCA_BUILD_ompi_coll_remap_DSO
component_noinst =
component_install = mca_coll_remap.la
else
component_noinst = libmca_coll_remap.la
component_install =
endif

# TODO: find a better solution for -L and -I scotch incldues

mcacomponentdir = $(ompilibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_coll_remap_la_SOURCES = $(sources)
mca_coll_remap_la_LDFLAGS = -module -avoid-version -libverbs -L/home/bkitor/scotch-v6.1.0/lib/ -lscotch -lscotcherr
mca_coll_remap_la_CFLAGS = -I/home/bkitor/scotch-v6.1.0/include/
mca_coll_remap_la_LIBADD = $(top_builddir)/ompi/lib@OMPI_LIBMPI_NAME@.la

noinst_LTLIBRARIES = $(component_noinst)
libmca_coll_remap_la_SOURCES =$(sources)
libmca_coll_remap_la_LDFLAGS = -module -avoid-version -libverbs -L/home/bkitor/scotch-v6.1.0/lib/ -lscotch -lscotcherr
libmca_coll_remap_la_CFLAGS = -I/home/bkitor/scotch-v6.1.0/include/