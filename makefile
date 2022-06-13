run: build
	./ex3.out conf.txt

build:
	g++ main.cpp -pthread -o ex3.out

