README.H5Z-FPZIP

This package is a highly flexible floating point 
compression plugin for the HDF5 library using FPZIP compression.

For information about FPZIP compression and the BSD-Licensed FPZIP
library, see...

- https://computation.llnl.gov/projects/floating-point-compression

For information about HDF5 filter plugins, see...

- https://support.hdfgroup.org/HDF5/doc/Advanced/DynamicallyLoadedFilters

This H5Z-FPZIP plugin supports both FPZIP version 1.1.0.

This plugin uses the [*registered*](https://support.hdfgroup.org/services/filters.html#fpzip)
HDF5 plugin filter id 32014


This plugin supports 1, 2 and 3 dimensional
datasets of single and double precision integer and floating point data.
It can be applied to HDF5 datasets of more than 3 dimensions as  long as
no more than 3 dimensions of the chunking are of size greater than 1.


Before you use the package, you need to setup env parameters:

      setenv FPZIP_HOME   /absolutepath/fpzip-1.1.0
      setenv HDF5_HOME    /absolutepath/hdf5/1.8.11/intel/12.1.5
      setenv NETCDF_HOME  /absolutepath/netcdf-c-filters.dmh.install
   
      Note: only hdf5-1.8.11 and newer versions have plugin available

Compile the package:

      cd fpzip_plugin/src
      make all
      make install prefix=install

Test the package:

      setenv LD_LIBRARY_PATH $PWD/netcdf-c-filters.dmh.install/lib":$LD_LIBRARY_PATH"
      setenv HDF5_PLUGIN_PATH $PWD/fpzip_plugin/src/install/plugin
      cd fpzip_plugin/test
      make test_netcdf_plugin
      ./test_netcdf_plugin 

