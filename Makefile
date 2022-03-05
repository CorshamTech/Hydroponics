

CC=g++

all: Monitor sht30 ph pct2075

sht30: sht30.cpp
	$(CC) sht30.cpp -o sht30

ph: ph.cpp
	$(CC) ph.cpp -o ph

pct2075: pct2075.cpp
	$(CC) pct2075.cpp -o pct2075

Monitor: Monitor.cpp
	$(CC) Monitor.cpp -o Monitor

