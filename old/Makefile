Web: main.o util.o threadpool.o selfepoll.o requestData.o
	g++ *.o -lpthread
%.o:%.h %.c
	g++ -c %< -o %@
clean:
	rm -f *.o *.out