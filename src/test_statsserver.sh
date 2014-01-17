clang++ -Wall --include=../config.h  StatsServer.cpp TestStatsServer.cpp Log.cpp -lboost_system -lboost_thread -o test && ./test
