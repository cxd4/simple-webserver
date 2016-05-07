# Makefile for server
server:	server.c
	gcc server.c -Os -Wall -pedantic -o server
