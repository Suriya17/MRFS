all: fs.cpp fs.h testcase1.cpp testcase2.cpp testcase3.cpp testcase4.cpp
	g++ -o test1 fs.cpp testcase1.cpp
	g++ -o test2 fs.cpp testcase2.cpp
	g++ -o test3 fs.cpp testcase3.cpp
	g++ -o test4 fs.cpp testcase4.cpp

clean:
	rm test1 test2 test3 test4
