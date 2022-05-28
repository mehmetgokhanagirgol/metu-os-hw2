all:
	g++ -o hw2 hw2.cpp hw2_output.c -lpthread
clean:
	rm -f hw2