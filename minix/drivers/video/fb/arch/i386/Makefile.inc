# Makefile for arch-dependent framebuffer code (i386)
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

SRCS +=	fb_arch.c
