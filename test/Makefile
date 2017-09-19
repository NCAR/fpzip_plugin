include ../config.make

.PHONY: lib plugin 



plugin:
	cd ../src; $(MAKE) $(MAKEVARS) $@

lib:
	cd ../src; $(MAKE) $(MAKEVARS) $@

test_netcdf.o: test_filter.c
	$(CC) -c $< -o $@  $(CFLAGS) -I$(H5Z_FPZIP_BASE) -I$(FPZIP_INC) -I$(HDF5_INC) -I$(NETCDF_INC) 
	
test_netcdf_plugin: test_netcdf.o plugin
	$(CC) $< -o $@ $(PREPATH)$(HDF5_LIB) $(PREPATH)$(FPZIP_LIB) -L$(HDF5_LIB) -L$(FPZIP_LIB) -L$(NETCDF_LIB) -lhdf5 -lfpzip -lnetcdf -lm 



clean:
	rm -f test_netcdf.o test_netcdf_plugin
