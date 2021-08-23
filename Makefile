server: main.cpp ./threadpool/threadpool.h ./http/http.cpp ./http/http.h ./http/timer.cpp ./http/timer.h ./lock/locker.h ./log/log.cpp ./log/log.h 
	g++ -o server -g main.cpp ./threadpool/threadpool.h ./http/http.cpp ./http/http.h ./http/timer.cpp ./http/timer.h ./lock/locker.h ./log/log.cpp ./log/log.h  -lpthread


clean:
	rm  -r server