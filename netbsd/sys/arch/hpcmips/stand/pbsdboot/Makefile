#	$NetBSD: Makefile,v 1.5 2002/01/31 19:15:24 tv Exp $

UUDECODE?=	uudecode

all: pbsdboot.exe pbsdboot1.exe

pbsdboot.exe: pbsdboot.uu
	${UUDECODE} pbsdboot.uu
	chmod 755 pbsdboot.exe

pbsdboot1.exe: pbsdboot1.uu
	${UUDECODE} pbsdboot1.uu
	chmod 755 pbsdboot1.exe

uu::
	if [ X"`strings WMIPSDbg/pbsdboot.exe | grep 'Compiled for WinCE'`" = \
           X'Compiled for WinCE 1.01' ]; then \
          echo This binary is compiled for WinCE 1.01; \
          uuencode WMIPSDbg/pbsdboot.exe pbsdboot1.exe > pbsdboot1.uu; \
        else \
          echo This binary is compiled for WinCE 2.00; \
          uuencode WMIPSDbg/pbsdboot.exe pbsdboot.exe > pbsdboot.uu; \
        fi
