# $Id: Makefile,v 1.3 2005/01/07 19:59:57 mkern Exp $
all: sniff ares_ip2port ares_login

sniff: sniff.c
	gcc -g -Wall -O2 sniff.c -o sniff  -lnids -lnet -lpcap -lz
