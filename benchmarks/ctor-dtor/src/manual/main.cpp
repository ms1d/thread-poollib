#include <thread>

#ifndef NUM_TASKS
#define NUM_TASKS 8192
#endif

#ifndef NUM_THREADS
#define NUM_THREADS 32
#endif

void func(void) { }

int main() {
	std::thread pool[NUM_THREADS];
	for (auto &t : pool) {
		t = std::thread([] () {
			func();
		});
	}

	for (auto &t : pool) {
		t.join();
	}

	return 0;
}
