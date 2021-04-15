sources = \
        coll_remap.h \
        coll_remap_component.c \
        coll_remap_module.c \
        coll_remap_allreduce.c \
        coll_remap_bcast.c \
        coll_remap_topo_info.c


if MCA_BUILD_ompi_coll_remap_DSO
component_noinst =
component_install = mca_coll_remap.la
else
component_noinst = libmca_coll_remap.la
component_install =
endif

mcacomponentdir = $(ompilibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_coll_remap_la_SOURCES = $(sources)
mca_coll_remap_la_LDFLAGS = -module -avoid-version -libverbs
mca_coll_remap_la_LIBADD = $(top_builddir)/ompi/lib@OMPI_LIBMPI_NAME@.la

noinst_LTLIBRARIES = $(component_noinst)
libmca_coll_remap_la_SOURCES =$(sources)
libmca_coll_remap_la_LDFLAGS = -module -avoid-version -libverbs