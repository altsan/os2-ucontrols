# Makefile for ICC + NMAKE32 (tested with VAC 3.08 and Toolkit 4.52)
#

CC      = icc.exe
LINK    = ilink.exe
RC      = rc.exe
CFLAGS  = /Gm /Q /Ss /Sp /Wuse /Wpar
LFLAGS  = /NOE /PMTYPE:PM /NOLOGO /MAP
OBJS    = testapp.obj textctl.obj gpitext.obj gpiutil.obj byteparse.obj linebuf.obj textseq.obj debug.obj
LIBS    = libuls.lib libconv.lib
NAME    = testapp

# DEBUG  = 1
# Set environment variable DEBUG or uncomment line above to build with debugging symbols
!ifdef DEBUG
    OBJS = $(OBJS)
    CFLAGS = $(CFLAGS) /Ti+ /Tm+ /DDEBUG_LOG=\"debug.log\"
    LFLAGS = $(LFLAGS) /DEBUG
!endif

$(NAME).exe          : $(OBJS) $(NAME).res
                        $(LINK) $(LFLAGS) $(OBJS) $(LIBS) /OUT:$@
                        $(RC) $(NAME).res $@

$(NAME).res          : $(NAME).rc
                        $(RC) -r $(NAME).rc

testapp.obj          : gpitext.h gpiutil.h textctl.h

textctl.obj          : gpitext.h gpiutil.h byteparse.h debug.h

gpitext.obj          : gpitext.h byteparse.h debug.h

gpiutil.obj          : gpiutil.h debug.h

debug.obj            : debug.c debug.h

linebuf.obj          : linebuf.h

# Delete all binaries
clean                 :
                        rm -f $(OBJS) $(NAME).exe $(NAME).res *.map

