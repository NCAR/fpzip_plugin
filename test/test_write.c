#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hdf5.h"

#include "H5Zfpzip.h"

#define NAME_LEN 256

/* convenience macro to handle command-line args and help */
#define HANDLE_ARG(A,PARSEA,PRINTA,HELPSTR)                     \
{                                                               \
    int i;                                                      \
    char tmpstr[64];                                            \
    int len = snprintf(tmpstr, sizeof(tmpstr), "%s=" PRINTA, #A, A); \
    int len2 = strlen(#A)+1;                                    \
    for (i = 0; i < argc; i++)                                  \
    {                                                           \
        if (!strncmp(argv[i], #A"=", len2))                     \
        {                                                       \
            A = PARSEA;                                         \
            break;                                              \
        }                                                       \
    }                                                           \
    printf("    %s=" PRINTA " %*s\n",#A,A,60-len,#HELPSTR);     \
}

/* convenience macro to handle errors */
#define ERROR(FNAME)                                              \
do {                                                              \
    int _errno = errno;                                           \
    fprintf(stderr, #FNAME " failed at line %d, errno=%d (%s)\n", \
        __LINE__, _errno, _errno?strerror(_errno):"ok");          \
    return 1;                                                     \
} while(0)

/* Generate a simple, 1D sinusioidal data array with some noise */
#define TYPINT 1
#define TYPDBL 2
static int gen_data(size_t npoints, double noise, double amp, void **_buf, int typ)
{
    size_t i;
    double *pdbl = 0;
    int *pint = 0;
    printf("gendata %d\n",typ);
    /* create data buffer to write */
    if (typ == TYPINT)
        pint = (int *) malloc(npoints * sizeof(int));
    else
        pdbl = (double *) malloc(npoints * sizeof(double));
    srandom(0xDeadBeef);
    for (i = 0; i < npoints; i++)
    {
        double x = 2 * M_PI * (double) i / (double) (npoints-1);
        double n = noise * ((double) random() / ((double)(1<<31)-1) - 0.5);
        if (typ == TYPINT)
            pint[i] = (int) (amp * (1 + sin(x)) + n);
        else
            pdbl[i] = (double) (amp * (1 + sin(x)) + n);
    }
    if (typ == TYPINT)
        *_buf = pint;
    else
        *_buf = pdbl;
    return 0;
}

static int read_data(char const *fname, size_t npoints, double **_buf)
{
    size_t const nbytes = npoints * sizeof(double);
    int fd;

    if (0 > (fd = open(fname, O_RDONLY))) ERROR(open);
    if (0 == (*_buf = (double *) malloc(nbytes))) ERROR(malloc);
    if (nbytes != read(fd, *_buf, nbytes)) ERROR(read);
    if (0 != close(fd)) ERROR(close);
    return 0;
}

int main(int argc, char **argv)
{
    int i;

    /* filename variables */
    char *ifile = (char *) calloc(NAME_LEN,sizeof(char));
    char *ids   = (char *) calloc(NAME_LEN,sizeof(char));
    char *ofile = (char *) calloc(NAME_LEN,sizeof(char));
    char *zfile = (char *) calloc(NAME_LEN,sizeof(char));

    /* file data info */
    int ndims = 0;
    int dim1 = 0;
    int dim2 = 0;
    int dim3 = 0;

    /* sinusoid data generation variables */
    hsize_t npoints = 1024;
    double noise = 0.001;
    double amp = 17.7;
    int doint = 0;

    /* compression parameters (defaults taken from FPZIP header) */
    uint prec = 11;
    int *ibuf = 0;
    double *buf = 0;

    /* HDF5 related variables */
    hsize_t chunk = 256;
    hid_t fid, dsid, idsid, sid, cpid;
    unsigned int cd_values[10];
    int cd_nelmts = 10;

    /* compressed/uncompressed difference stat variables */
    double max_absdiff = 0;
    double max_reldiff = 0;
    int num_diffs = 0;
    
    /* file arguments */
    HANDLE_ARG(ifile,strndup(argv[i]+len2,NAME_LEN), "\"%s\"",set input filename);
    HANDLE_ARG(ids,strndup(argv[i]+len2,NAME_LEN), "\"%s\"",set input datast name);
    HANDLE_ARG(ofile,strndup(argv[i]+len2,NAME_LEN), "\"%s\"",set output filename);
    HANDLE_ARG(zfile,strndup(argv[i]+len2,NAME_LEN), "\"%s\"",set compressed filename);

    /* data generation arguments */
    HANDLE_ARG(npoints,(hsize_t) strtol(argv[i]+len2,0,10), "%llu",set number of points for generated dataset);
    HANDLE_ARG(noise,(double) strtod(argv[i]+len2,0),"%g",set amount of random noise in generated dataset);
    HANDLE_ARG(amp,(double) strtod(argv[i]+len2,0),"%g",set amplitude of sinusoid in generated dataset);
    HANDLE_ARG(doint,(int) strtol(argv[i]+len2,0,10),"%d",also do integer data);

    /* HDF5 chunking and ZFP filter arguments */
    HANDLE_ARG(chunk,(hsize_t) strtol(argv[i]+len2,0,10), "%llu",set chunk size for dataset);
    HANDLE_ARG(prec,(uint) strtol(argv[i]+len2,0,10),"%u",set precision for precision mode of fpzip filter);

    /* setup output filename if not already specified */
    if (ofile[0] == '\0')
        strncpy(ofile, "test_fpzip.h5", NAME_LEN);

    /* create double data to write if we're not reading from an existing file */
    if (ifile[0] == '\0')
        gen_data((size_t) npoints, noise, amp, (void**)&buf, TYPDBL);
    else
        read_data(ifile, (size_t) npoints, &buf);

    /* create integer data to write */
    if (doint)
        gen_data((size_t) npoints, noise*100, amp*1000000, (void**)&ibuf, TYPINT);

    /* setup dataset creation properties */
    if (0 > (cpid = H5Pcreate(H5P_DATASET_CREATE))) ERROR(H5Pcreate);
    if (0 > H5Pset_chunk(cpid, 1, &chunk)) ERROR(H5Pset_chunk);
    
    /* setup fpzip filter via generic (cd_values) interface */
    H5Pset_fpzip_precision_cdata(prec, cd_nelmts, cd_values);

    /* print cd-values array used for filter */
    printf("%d cd_values= ",cd_nelmts);
    for (i = 0; i < cd_nelmts; i++)
        printf("%u,", cd_values[i]);
    printf("\n");

    /* exit if help is requested (do this here instead of earlier to permit
       use of test_write to print cd_values array for a invokation of h5repack). */
    for (i = 1; i < argc; i++)
        if (strcasestr(argv[i],"help")!=0) return 0;

    /* Add filter to the pipeline via generic interface */
    if (0 > H5Pset_filter(cpid, H5Z_FILTER_FPZIP, H5Z_FLAG_MANDATORY, cd_nelmts, cd_values)) ERROR(H5Pset_filter);


    /* create HDF5 file */
    if (0 > (fid = H5Fcreate(ofile, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT))) ERROR(H5Fcreate);

    /* setup the 1D data space */
    if (0 > (sid = H5Screate_simple(1, &npoints, 0))) ERROR(H5Screate_simple);

    /* write the data WITHOUT compression */
    if (0 > (dsid = H5Dcreate(fid, "original", H5T_NATIVE_DOUBLE, sid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT))) ERROR(H5Dcreate);
    if (0 > H5Dwrite(dsid, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf)) ERROR(H5Dwrite);
    if (0 > H5Dclose(dsid)) ERROR(H5Dclose);
    if (doint)
    {
        if (0 > (idsid = H5Dcreate(fid, "int_original", H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT))) ERROR(H5Dcreate);
        if (0 > H5Dwrite(idsid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, ibuf)) ERROR(H5Dwrite);
        if (0 > H5Dclose(idsid)) ERROR(H5Dclose);
    }

    /* write the data with requested compression */
    if (0 > (dsid = H5Dcreate(fid, "compressed", H5T_NATIVE_DOUBLE, sid, H5P_DEFAULT, cpid, H5P_DEFAULT))) ERROR(H5Dcreate);
    if (0 > H5Dwrite(dsid, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf)) ERROR(H5Dwrite);
    if (0 > H5Dclose(dsid)) ERROR(H5Dclose);
    if (doint)
    {
        if (0 > (idsid = H5Dcreate(fid, "int_compressed", H5T_NATIVE_INT, sid, H5P_DEFAULT, cpid, H5P_DEFAULT))) ERROR(H5Dcreate);
        if (0 > H5Dwrite(idsid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, ibuf)) ERROR(H5Dwrite);
        if (0 > H5Dclose(idsid)) ERROR(H5Dclose);
    }

    /* clean up */
    if (0 > H5Sclose(sid)) ERROR(H5Sclose);
    if (0 > H5Pclose(cpid)) ERROR(H5Pclose);
    if (0 > H5Fclose(fid)) ERROR(H5Fclose);

    free(buf);
    free(ifile);
    free(ofile);
    free(zfile);
    free(ids);

    H5close();

    return 0;
}
