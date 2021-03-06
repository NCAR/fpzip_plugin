#include <stdlib.h>
#include <string.h>

#include "H5Zfpzip.h"
#include "H5PLextern.h"
#include "H5Spublic.h"
#include "fpzip.h"


#define FPZIP_FP 1        /* Have to define FPZIP_FP here to include codec.h, this is a temporary solution*/
#include "codec.h" 

/* FPZIP version numbers */
#define FPZIP_VERSION_NO                   FPZ_MAJ_VERSION 

/* Convenient CPP logic to capture H5Z_FPZIP Filter version numbers as string and hex number */
#define H5Z_FILTER_FPZIP_VERSION_STR__(Maj,Min,Pat) #Maj "." #Min "." #Pat
#define H5Z_FILTER_FPZIP_VERSION_STR_(Maj,Min,Pat)  H5Z_FILTER_FPZIP_VERSION_STR__(Maj,Min,Pat)
#define H5Z_FILTER_FPZIP_VERSION_STR                H5Z_FILTER_FPZIP_VERSION_STR_(H5Z_FILTER_FPZIP_VERSION_MAJOR,H5Z_FILTER_FPZIP_VERSION_MINOR,H5Z_FILTER_FPZIP_VERSION_PATCH)

#define H5Z_FILTER_FPZIP_VERSION_NO__(Maj,Min,Pat)  (0x0 ## Maj ## Min ## Pat)
#define H5Z_FILTER_FPZIP_VERSION_NO_(Maj,Min,Pat)   H5Z_FILTER_FPZIP_VERSION_NO__(Maj,Min,Pat)
#define H5Z_FILTER_FPZIP_VERSION_NO                 H5Z_FILTER_FPZIP_VERSION_NO_(H5Z_FILTER_FPZIP_VERSION_MAJOR,H5Z_FILTER_FPZIP_VERSION_MINOR,H5Z_FILTER_FPZIP_VERSION_PATCH)


#define H5Z_FPZIP_PUSH_AND_GOTO(MAJ, MIN, RET, MSG)     \
do                                                    \
{                                                     \
    H5Epush(H5E_DEFAULT,__FILE__,_funcname_,__LINE__, \
        H5E_ERR_CLS_g,MAJ,MIN,MSG);                \
    retval = RET;                                     \
    goto done;                                        \
} while(0)

size_t H5Z_filter_fpzip   (unsigned int flags, size_t cd_nelmts, const unsigned int cd_values[],
                                size_t nbytes, size_t *buf_size, void **buf);
htri_t H5Z_fpzip_can_apply(hid_t dcpl_id, hid_t type_id, hid_t space_id);
herr_t H5Z_fpzip_set_local(hid_t dcpl_id, hid_t type_id, hid_t space_id);

const H5Z_class2_t H5Z_FPZIP[1] = {{

    H5Z_CLASS_T_VERS,       /* H5Z_class_t version          */
    H5Z_FILTER_FPZIP,       /* Filter id number             */
    1,                      /* encoder_present flag         */
    1,                      /* decoder_present flag         */
    "fpzip",
    H5Z_fpzip_can_apply,      /* The "can apply" callback     */
    H5Z_fpzip_set_local,      /* The "set local" callback     */
    H5Z_filter_fpzip,         /* The actual filter function   */

}};

H5PL_type_t H5PLget_plugin_type(void) {return H5PL_TYPE_FILTER;}
const void *H5PLget_plugin_info(void) {return H5Z_FPZIP;}

hid_t H5Z_FPZIP_ERRCLASS = -1;


htri_t
H5Z_fpzip_can_apply(hid_t dcpl_id, hid_t type_id, hid_t chunk_space_id)
{   
    static char const *_funcname_ = "H5Z_fpzip_can_apply";
    int ndims, ndims_used = 0;
    size_t i, dsize;
    htri_t retval = 0;
    hsize_t dims[H5S_MAX_RANK];
    H5T_class_t dclass;
    hid_t native_type_id;

    /* get datatype class, size and space dimensions */
    if (H5T_NO_CLASS == (dclass = H5Tget_class(type_id)))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADTYPE, -1, "bad datatype class");

    if (0 == (dsize = H5Tget_size(type_id)))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADTYPE, -1, "bad datatype size");

    if (0 > (ndims = H5Sget_simple_extent_dims(chunk_space_id, dims, 0)))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADTYPE, -1, "bad chunk data space");

    /* confirm FPZIP library can handle this data */
    if (!(dclass == H5T_FLOAT ))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADTYPE, 0,
            "requires datatype class of H5T_FLOAT or H5T_DOUBLE");

    if (!(dsize == 4 || dsize == 8))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADTYPE, 0,
            "requires datatype size of 4 or 8");

    /* check for *USED* dimensions of the chunk */
    for (i = 0; i < ndims; i++)
    {
        if (dims[i] <= 1) continue;
        ndims_used++;
    }

    if (ndims_used == 0 || ndims_used > 3)
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADVALUE, 0,
            "chunk must have only 1-3 non-unity dimensions");


    retval = 1;

done:

    return retval;
}

herr_t
H5Z_fpzip_set_local(hid_t dcpl_id, hid_t type_id, hid_t chunk_space_id)
{   
    static char const *_funcname_ = "H5Z_fpzip_set_local";
    int i, ndims, ndims_used = 0;
    size_t dsize, hdr_bits, hdr_bytes;
    size_t mem_cd_nelmts = H5Z_FPZIP_CD_NELMTS_MEM;
    unsigned int ma,mi,re,mem_cd_values[H5Z_FPZIP_CD_NELMTS_MEM];
    size_t hdr_cd_nelmts = H5Z_FPZIP_CD_NELMTS_MAX;
    unsigned int hdr_cd_values[H5Z_FPZIP_CD_NELMTS_MAX];
    unsigned char header_buffer[1024];
    unsigned int flags = 0;
    herr_t retval = 0;
    hsize_t dims[H5S_MAX_RANK], dims_used[3];
    H5T_class_t dclass;
    int zt;
    FPZ* fpz;

    if (0 > (dclass = H5Tget_class(type_id)))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_ARGS, H5E_BADTYPE, -1, "not a datatype");

    if (0 == (dsize = H5Tget_size(type_id)))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_ARGS, H5E_BADTYPE, -1, "not a datatype");

    if (0 > (ndims = H5Sget_simple_extent_dims(chunk_space_id, dims, 0)))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_ARGS, H5E_BADTYPE, -1, "not a data space");

    for (i = 0; i < ndims; i++)
    {
        if (dims[i] <= 1) continue;
        dims_used[ndims_used] = dims[i];
        ndims_used++;
    }

    /* setup fpzip data type for meta header */
    if (dclass == H5T_FLOAT)
    {
        zt = (dsize == 4) ? FPZIP_TYPE_FLOAT : FPZIP_TYPE_DOUBLE;
    }
    else
    {
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADTYPE, 0,
            "datatype class must be H5T_FLOAT or H5T_DOUBLE");
    }

    /* Into hdr_cd_values, we encode FPZIP library and H5Z-FPZIP plugin version info at
       entry 0 and use remaining entries as a tiny buffer to write fpzip native header. */
    hdr_cd_values[0] = (unsigned int) ((FPZIP_VERSION_NO<<16) | H5Z_FILTER_FPZIP_VERSION_NO);
    fpz = fpzip_write_to_buffer((unsigned char*)&hdr_cd_values[1],sizeof(hdr_cd_values)-sizeof(hdr_cd_values[0]));
    if (!fpz)
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "fpzip_write_to_buffer() failed");

    /* set up fpz to store meta header */
    switch (ndims_used)
    {
        case 1: fpz->nx=dims_used[0];fpz->ny=1;fpz->nz=1;fpz->nf=1;break;
        case 2: fpz->nx=dims_used[0];fpz->ny=dims_used[1];fpz->nz=1;fpz->nf=1; break;
        case 3: fpz->nx=dims_used[0];fpz->ny=dims_used[1];fpz->nz=dims_used[2];fpz->nf=1; break;
        default: H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADVALUE, 0,
                     "requires dimensions in 1,2 or 3");
    }

    /* get current cd_values and re-map to new cd_value set */
    if (0 > H5Pget_filter_by_id(dcpl_id, H5Z_FILTER_FPZIP, &flags, &mem_cd_nelmts, mem_cd_values, 0, NULL, NULL))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_CANTGET, 0, "unable to get current FPZIP cd_values");

    /* Set the fpz precision value from mem_cd_values[0] */
    fpz->prec = mem_cd_values[0];
    fpz->type = zt;

    /* Use FPZIP's write_header method to write the fpz header into hdr_cd_values array */
    if (!fpzip_write_header(fpz)) 
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_CANTINIT, 0, "unable to write header");

    /* Now, update cd_values for the filter */
    if (0 > H5Pmodify_filter(dcpl_id, H5Z_FILTER_FPZIP, flags, hdr_cd_nelmts, hdr_cd_values))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADVALUE, 0,
            "failed to modify cd_values");
    retval = 1;
done:
    return retval;
}

static int
get_fpzip_info_from_cd_values_0x0110(size_t cd_nelmts, unsigned int const *cd_values,
    FPZ **fpz )
{
    static char const *_funcname_ = "get_fpzip_info_from_cd_values_0x0110";
    int retval = 0;

    if (cd_nelmts > H5Z_FPZIP_CD_NELMTS_MAX)
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_OVERFLOW, 0, "cd_nelmts exceeds max");


    /* treat the cd_values as a fpz buffer */
    *fpz = fpzip_read_from_buffer((unsigned char*)&cd_values[0]);
    if (!(*fpz))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "fpzip read from buffer failed");

    if (!fpzip_read_header(*fpz)) 
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "fpzip read header failed");

    retval = 1;

done:

    return retval;
}

/* Decode cd_values for FPZIP info for various versions of this filter */
int
get_fpzip_info_from_cd_values(size_t cd_nelmts, unsigned int const *cd_values,
    FPZ** fpz)
{
    unsigned int const h5z_fpzip_version_no = cd_values[0]&0x0000FFFF;
    int retval = 0;

    if (0x0110 <= h5z_fpzip_version_no && h5z_fpzip_version_no <= 0x0120)
        retval=get_fpzip_info_from_cd_values_0x0110(cd_nelmts, &cd_values[1], fpz);

    //if (retval == 0)
    H5Epush(H5E_DEFAULT, __FILE__, "", __LINE__, H5E_ERR_CLS_g, H5E_PLINE, H5E_BADVALUE,
        "version mismatch: (file) 0x0%x <-> 0x0%x (code)", h5z_fpzip_version_no, H5Z_FILTER_FPZIP_VERSION_NO);

    return retval;
}

size_t
H5Z_filter_fpzip(unsigned int flags, size_t cd_nelmts,
    const unsigned int cd_values[], size_t nbytes,
    size_t *buf_size, void **buf)
{
    static char const *_funcname_ = "H5Z_filter_fpzip";
    void *newbuf = 0;
    size_t retval = 0;
    int cd_vals_fpzipver = (cd_values[0]>>16)&0x0000FFFF;
    FPZ* fpz;
    FPZ* fpz_real = 0;
    size_t inbytes = 0;

    if (0 == get_fpzip_info_from_cd_values(cd_nelmts, cd_values, &fpz))
        H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_CANTGET, 0, "can't get FPZIP mode/meta");
    

    if (flags & H5Z_FLAG_REVERSE) /* decompression */
    {
        int status;
        size_t bsize =0, dsize;

        
        if (0 == (fpz_real = fpzip_read_from_buffer(*buf)))
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "read from buffer failed");
        if (0 == (fpzip_read_header(fpz_real))) 
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "read header failed");
        
        bsize = fpz_real->nx*fpz_real->ny*fpz_real->nz*fpz_real->nf;
        
        switch (fpz_real->type)
        {
            case FPZIP_TYPE_FLOAT: dsize = 4; break;
            case FPZIP_TYPE_DOUBLE: dsize = 8; break;
            default: H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_BADTYPE, 0, "invalid datatype");
        }
        bsize *= dsize;
        
        if (NULL == (newbuf = malloc(bsize)))
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0,
                "memory allocation failed for FPZIP decompression"); 
        
        /* Do the FPZIP decompression operation */
        if (0 == (fpzip_read(fpz_real, newbuf))) 
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "decompression failed");

        /* clean up */
        fpzip_read_close(fpz_real);


        free(*buf);
        *buf = newbuf;
        newbuf = 0;
        *buf_size = bsize; 
        retval = bsize;
    }
    else /* compression */
    {
        size_t bsize = 0, dsize, out_size;

        dsize = (fpz->type == FPZIP_TYPE_FLOAT ? sizeof(float) : sizeof(double));
        inbytes = (size_t)fpz->nx*fpz->ny*fpz->nz*fpz->nf*dsize;
        bsize = inbytes + 1024;

        /* Set up the FPZIP field object */
        if (NULL == (newbuf=malloc(bsize)))
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "newbuf malloc failed");

        /* Set up the FPZIP stream object for real compression now */
        if (0 == (fpz_real = fpzip_write_to_buffer(newbuf, bsize)))
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "fpzip write to buffer failed");

        fpz_real->type=fpz->type;
        fpz_real->prec=fpz->prec;
        fpz_real->nx=fpz->nx;
        fpz_real->ny=fpz->ny;
        fpz_real->nz=fpz->nz;
        fpz_real->nf=fpz->nf;

        /* Set up the bitstream object */
        if (0 == (fpzip_write_header(fpz_real)))
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_NOSPACE, 0, "fpzip write header failed");


        /* Do the compression */
        out_size = fpzip_write(fpz_real,*buf);

        /* clean up */
        fpzip_write_close(fpz_real);

        if (out_size == 0)
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_PLINE, H5E_CANTFILTER, 0, "compression failed");

        if (out_size > inbytes)
            H5Z_FPZIP_PUSH_AND_GOTO(H5E_RESOURCE, H5E_OVERFLOW, 0, "uncompressed buffer overrun");

        free(*buf);
        *buf = newbuf;
        newbuf = 0;
        *buf_size = out_size;
        retval = out_size;
    }

done:
    return retval ;
}

