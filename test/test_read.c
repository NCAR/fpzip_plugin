/*
    Copyright (c) 2016, Lawrence Livermore National Security, LLC.
        Produced at the Lawrence Livermore National Laboratory
           Written by Mark C. Miller, miller86@llnl.gov
               LLNL-CODE-707197 All rights reserved.

This file  is part  of H5Z-ZFP.  For details, see
https://github.com/LLNL/H5Z-ZFP.  Please  also  read  the   Additional
BSD Notice.

Redistribution and  use in  source and binary  forms, with  or without
modification, are permitted provided that the following conditions are
met:

* Redistributions  of  source code  must  retain  the above  copyright
  notice, this list of conditions and the disclaimer below.

* Redistributions in  binary form  must reproduce the  above copyright
  notice, this list of conditions  and the disclaimer (as noted below)
  in  the  documentation  and/or  other materials  provided  with  the
  distribution.

* Neither the name of the  LLNS/LLNL nor the names of its contributors
  may  be  used to  endorse  or  promote  products derived  from  this
  software without specific prior written permission.

THIS SOFTWARE  IS PROVIDED BY  THE COPYRIGHT HOLDERS  AND CONTRIBUTORS
"AS  IS" AND  ANY EXPRESS  OR IMPLIED  WARRANTIES, INCLUDING,  BUT NOT
LIMITED TO, THE IMPLIED  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A  PARTICULAR  PURPOSE ARE  DISCLAIMED.  IN  NO  EVENT SHALL  LAWRENCE
LIVERMORE  NATIONAL SECURITY, LLC,  THE U.S.  DEPARTMENT OF  ENERGY OR
CONTRIBUTORS BE LIABLE FOR  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR  CONSEQUENTIAL DAMAGES  (INCLUDING, BUT NOT  LIMITED TO,
PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS  OF USE,  DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER  IN CONTRACT, STRICT LIABILITY,  OR TORT (INCLUDING
NEGLIGENCE OR  OTHERWISE) ARISING IN  ANY WAY OUT  OF THE USE  OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Additional BSD Notice

1. This notice is required to  be provided under our contract with the
U.S. Department  of Energy (DOE).  This work was produced  at Lawrence
Livermore  National Laboratory  under  Contract No.  DE-AC52-07NA27344
with the DOE.

2.  Neither  the  United  States  Government  nor  Lawrence  Livermore
National Security, LLC nor any of their employees, makes any warranty,
express or implied, or assumes any liability or responsibility for the
accuracy, completeness,  or usefulness of  any information, apparatus,
product, or  process disclosed, or  represents that its use  would not
infringe privately-owned rights.

3.  Also,  reference  herein  to  any  specific  commercial  products,
process,  or  services  by  trade  name,  trademark,  manufacturer  or
otherwise does  not necessarily  constitute or imply  its endorsement,
recommendation,  or  favoring  by  the  United  States  Government  or
Lawrence Livermore  National Security, LLC. The views  and opinions of
authors expressed herein do not  necessarily state or reflect those of
the United States Government  or Lawrence Livermore National Security,
LLC,  and shall  not be  used for  advertising or  product endorsement
purposes.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hdf5.h"

#include "H5Zfpzip.h"

#define NAME_LEN 256

/* convenience macro to handle command-line args */
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
#define ERROR(FNAME)                                            \
do {                                                            \
    fprintf(stderr, #FNAME " failed at line %d\n", __LINE__);   \
    return 1;                                                   \
} while(0)

int main(int argc, char **argv)
{
    int i;
    double *obuf, *cbuf;

    /* filename variables */
    char *ifile = (char *) calloc(NAME_LEN,sizeof(char));

    /* HDF5 dataset info */
    hid_t fid, dsid, dcpl_id, space_id;
    hsize_t npoints;

    /* compressed/uncompressed difference stat variables */
    double max_absdiff = 0;
    double max_reldiff = 0;
    int num_diffs = 0;
    
    /* file arguments */
    HANDLE_ARG(ifile,strndup(argv[i]+len2,NAME_LEN), "\"%s\"",set input filename);

    /* exit if help is requested */
    for (i = 1; i < argc; i++)
        if (strcasestr(argv[i],"help")!=0) return 0;

    /* setup input filename if not already specified */
    if (ifile[0] == '\0')
        strncpy(ifile, "test_fpzip.h5", NAME_LEN);


    /* open the HDF5 file */
    if (0 > (fid = H5Fopen(ifile, H5F_ACC_RDONLY, H5P_DEFAULT))) ERROR(H5Fopen);

    /* read the original dataset */
    if (0 > (dsid = H5Dopen(fid, "original", H5P_DEFAULT))) ERROR(H5Dopen);
    if (0 > (space_id = H5Dget_space(dsid))) ERROR(H5Dget_space);
    if (0 == (npoints = H5Sget_simple_extent_npoints(space_id))) ERROR(H5Sget_simple_extent_npoints);
    if (0 > H5Sclose(space_id)) ERROR(H5Sclose);
    if (0 == (obuf = (double *) malloc(npoints * sizeof(double)))) ERROR(malloc);
    if (0 > H5Dread(dsid, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, obuf)) ERROR(H5Dread);
    if (0 > H5Dclose(dsid)) ERROR(H5Dclose);
    
    /* read the compressed dataset */
    if (0 > (dsid = H5Dopen(fid, "compressed", H5P_DEFAULT))) ERROR(H5Dopen);
    if (0 > (dcpl_id = H5Dget_create_plist(dsid))) ERROR(H5Dget_create_plist);
    if (0 == (cbuf = (double *) malloc(npoints * sizeof(double)))) ERROR(malloc);
    if (0 > H5Dread(dsid, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, cbuf)) ERROR(H5Dread);
    if (0 > H5Dclose(dsid)) ERROR(H5Dclose);

    /* clean up */
    if (0 > H5Pclose(dcpl_id)) ERROR(H5Pclose);
    if (0 > H5Fclose(fid)) ERROR(H5Fclose);

    /* compare original to compressed */
    for (i = 0; i < npoints; i++)
    {
        double absdiff = obuf[i] - cbuf[i];
        if (absdiff < 0) absdiff = -absdiff;
        if (absdiff > 0)
        {
            double reldiff = 0;
            if (obuf[i] != 0) reldiff = absdiff / obuf[i];

            if (absdiff > max_absdiff) max_absdiff = absdiff;
            if (reldiff > max_reldiff) max_reldiff = reldiff;
            num_diffs++;
        }
    }
    printf("%d values are different; max-absdiff = %g, max-reldiff = %g\n",
        num_diffs, max_absdiff, max_reldiff);

    free(obuf);
    free(cbuf);
    free(ifile);

    return 0;
}
