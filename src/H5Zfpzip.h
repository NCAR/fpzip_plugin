//#include "hdf5.h"

/* Filter ID number registered with The HDF Group */
#define H5Z_FILTER_FPZIP 32014

#define H5Z_FILTER_FPZIP_VERSION_MAJOR 1
#define H5Z_FILTER_FPZIP_VERSION_MINOR 1
#define H5Z_FILTER_FPZIP_VERSION_PATCH 0


#define H5Z_FPZIP_CD_NELMTS_MEM ((size_t) 7) /* used in public API to filter */
#define H5Z_FPZIP_CD_NELMTS_MAX ((size_t) 7) /* max, over all versions, used in dataset header */

#define H5Pset_fpzip_precision_cdata(P, N, CD)  \
do { if (N>=3) {CD[0]=CD[1]=CD[2];            \
CD[0]=P;                 \
CD[2]=0; N=3;}} while(0)

size_t H5Z_filter_fpzip   (unsigned int flags, size_t cd_nelmts, const unsigned int cd_values[],
                                size_t nbytes, size_t *buf_size, void **buf);

