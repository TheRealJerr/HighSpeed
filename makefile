bin : main.cpp
	g++ -o bin main.cpp -lpthread -std=c++23 -Wall

.PHONY : clean
clean :
	rm bin