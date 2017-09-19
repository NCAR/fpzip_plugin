include ../config.make

# The filter compiled to be used as a plugin
H5Zfpzip_plugin.o: H5Zfpzip.c
	$(CC) -c $< -o $@ $(CFLAGS) -I$(H5Z_FPZIP_BASE) -I$(FPZIP_INC) -I$(HDF5_INC)

# The filter compiled to be used as a library
H5Zfpzip_lib.o: H5Zfpzip.c
	$(CC) -c $< -o $@ -DH5Z_FPZIP_AS_LIB $(CFLAGS) -I$(H5Z_FPZIP_BASE) -I$(FPZIP_INC) -I$(HDF5_INC)

# The plugin shared lib (HDF5 expects it to be named "lib*.{so,dylib}"
plugin/libh5zfpzip.$(SOEXT): H5Zfpzip_plugin.o
	rm -rf plugin
	mkdir plugin
	$(CC) $< $(SHFLAG) -o plugin/libh5zfpzip.$(SOEXT) \
	    $(PREPATH)$(HDF5_LIB) $(PREPATH)$(FPZIP_LIB) \
	    -L$(FPZIP_LIB) -L$(HDF5_LIB) -lhdf5 -lfpzip $(LDFLAGS)

# Alias target for filter plugin
plugin: plugin/libh5zfpzip.$(SOEXT)







all: plugin

install: all
	$(INSTALL) -d $(PREFIX)/{plugin,include}
	$(INSTALL) plugin/libh5zfpzip.$(SOEXT) $(PREFIX)/plugin
	$(INSTALL) -m 644 H5Zfpzip.h $(PREFIX)/include

clean:
	rm -rf plugin libh5zfpzip.a *.o *.mod
