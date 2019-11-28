# -------------------------------------------------------------------------
# choose your compiler (must be ANSI-compliant!) and linker command, plus
# any additionally needed flags
         
CC = cc
LD = cc 
CFLAGS = -xO4 -xcg92

TARG_OBJEXTENSION = .o

HOST_OBJEXTENSION = $(TARG_OBJEXTENSION)

# -------------------------------------------------------------------------
# directories where binaries, includes, and manpages should go during
# installation

BINDIR = /usr/local/bin
INCDIR = /usr/local/include/asl
MANDIR = /usr/local/man
LIBDIR = /usr/local/lib/asl
DOCDIR = /usr/local/doc/asl

# -------------------------------------------------------------------------
# character encoding to use (choose one of them)

CHARSET = CHARSET_ISO8859_1
# CHARSET = CHARSET_ASCII7
# CHARSET = CHARSET_IBM437
