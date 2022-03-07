#############################################################
# Check submodules
#
SUBMODULES := ../libesphttpd/Makefile 

submodules: $(SUBMODULES) 

$(SUBMODULES):
	@echo No submodule
	cd ..; git clone https://github.com/slacky1965/libesphttpd.git
	mv ../libesphttpd/Makefile ../libesphttpd/Makefile.orig
	cp Makefile.libesphttpd ../libesphttpd/Makefile
	cp Makefile.libesphttpd.core ../libesphttpd/core/Makefile
#	git submodule init
#	git submodule update
	
#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

#INCLUDES := $(INCLUDES) -I $(PDIR)include

PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile
