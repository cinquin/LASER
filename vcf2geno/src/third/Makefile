# download all third party library and compile them

LIB = tabix pcre eigen gsl bzip2
all: $(LIB)

tabix:
	ln -s -f tabix-0.2.6 tabix
	(cd tabix-0.2.6; $(MAKE); $(MAKE) libtabix.so.1)

bzip2:
	ln -s -f bzip2-1.0.6 bzip2
	(cd bzip2-1.0.6; $(MAKE))

######################################################################
clean:
	-rm -rf $(LIB)
deepclean:
	-rm -rf $(LIB)
	-rm -rf pcre-8.21
	-rm -rf gsl-1.15
	-rm -rf eigen-3.1.0
	(cd tabix-0.2.6; $(MAKE) clean)
	(cd bzip2-1.0.6; $(MAKE) clean)

