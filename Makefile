include config.make

.PHONY: help all clean dist install

all:
	cd src; $(MAKE) $(MAKEVARS) $@

test:
	cd test; $(MAKE) $(MAKEVARS) $@

install:
	cd src; $(MAKE) $(MAKEVARS) $@

clean:
	rm -f H5Z-FPZIP-$(H5Z_FPZIP_VERSINFO).tar.gz
	cd src; $(MAKE) $(MAKEVARS) $@
	cd test; $(MAKE) $(MAKEVARS) $@

dist:	clean
	rm -rf H5Z-FPZIP-$(H5Z_FPZIP) H5Z-FPZIP-$(H5Z_FPZIP_VERSINFO).tar.gz; \
	mkdir H5Z-FPZIP-$(H5Z_FPZIP_VERSINFO); \
	tar cf - --exclude ".git*" --exclude H5Z-FPZIP-$(H5Z_FPZIP_VERSINFO) . | tar xf - -C H5Z-FPZIP-$(H5Z_FPZIP_VERSINFO); \
	tar cvf - H5Z-FPZIP-$(H5Z_FPZIP_VERSINFO) | gzip --best > H5Z-FPZIP-$(H5Z_FPZIP_VERSINFO).tar.gz; \
	rm -rf H5Z-FPZIP-$(H5Z_FPZIP_VERSINFO);
