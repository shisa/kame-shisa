
# $FreeBSD: src/sys/modules/aic7xxx/ahd/Makefile,v 1.4 2003/01/22 21:56:54 gibbs Exp $

.PATH:	${.CURDIR}/../../../dev/aic7xxx
KMOD=	ahd

GENSRCS= aic79xx_{seq,reg}.h
REG_PRINT_OPT=
AHD_REG_PRETTY_PRINT=1
.ifdef AHD_REG_PRETTY_PRINT
GENSRCS += aic79xx_reg_print.c
CFLAGS+= -DAHD_REG_PRETTY_PRINT=1
REG_PRINT_OPT= -p aic79xx_reg_print.c
.endif
BEFORE_DEPEND= ${GENSRCS}

../aicasm/aicasm: ${.CURDIR}/../../../dev/aic7xxx/aicasm/*.[chyl]
	( cd ${.CURDIR}/../aicasm; ${MAKE} aicasm; )

${GENSRCS}:								 \
		${.CURDIR}/../../../dev/aic7xxx/aic79xx.{reg,seq}	 \
		${.CURDIR}/../../../cam/scsi/scsi_message.h ../aicasm/aicasm
	../aicasm/aicasm ${INCLUDES} -I${.CURDIR}/../../../cam/scsi	 \
			-I${.CURDIR}/../../../dev/aic7xxx		 \
			-o aic79xx_seq.h -r aic79xx_reg.h		 \
			${REG_PRINT_OPT}				 \
			-i ${.CURDIR}/../../../dev/aic7xxx/aic79xx_osm.h \
			${.CURDIR}/../../../dev/aic7xxx/aic79xx.seq

SRCS= ${GENSRCS}
SRCS+= aic79xx.c aic79xx_osm.c aic79xx_pci.c ahd_pci.c
SRCS+= opt_scsi.h opt_aic79xx.h opt_cam.h opt_ddb.h
SRCS+= device_if.h bus_if.h pci_if.h

CLEANFILES= ${GENSRCS}

.include <bsd.kmod.mk>
