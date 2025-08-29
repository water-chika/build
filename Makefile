all: test
	./test
test:
	c++ test.cpp -std=c++20 -o test -g
