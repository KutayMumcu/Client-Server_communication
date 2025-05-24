# Makefile for Homework 3 - Chat Server

CC=gcc
CFLAGS=-Wall -pthread

all: echo_server echo_client

echo_server: echo_server.c
	$(CC) $(CFLAGS) -o echo_server echo_server.c

echo_client: echo_client.c
	$(CC) $(CFLAGS) -o echo_client echo_client.c

clean:
	rm -f echo_server echo_client
