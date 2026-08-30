#pragma once



#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <mutex>



enum class pool_type {
    mutex = 0,		// Single mutex protects the entire buffer
    vyukov_spin = 1,			// Vyukov-style two-pointer + sequence number approach
    vyukov_idle = 2,			// Similar to above but threads idle instead of spinning
    work_stealing_buffer = 3		// Typical decentralized work-stealing approach
};



template<class obj, uint32_t len, pool_type type = pool_type::mutex>
struct mpmc;

template<class obj, uint32_t len>
struct mpmc<obj, len, pool_type::mutex> {
	alignas(64) uint32_t head, tail;
	obj *buffer[len];
	std::mutex mtx;
	std::condition_variable cv_not_empty, cv_not_full;
    bool stop = false;

	// MUST be called before dtor
	void shutdown() {
		{
			std::lock_guard<std::mutex> l(mtx);
			stop = true;
		}

		cv_not_full.notify_all();
        cv_not_empty.notify_all();
	}
	
	mpmc() = default;

	~mpmc() { assert(stop); }

	bool submit(obj *object) {
		std::unique_lock<std::mutex> l(mtx);
		cv_not_full.wait(l, [this] () {
			return stop || tail - head < len;
		});

		if (stop) return false;

		buffer[tail % len] = object;
		tail++;

		l.unlock();
		cv_not_empty.notify_one();

		return true;
	}

	bool try_submit(obj *object) {
		{
			std::lock_guard<std::mutex> l(mtx);
			if (stop || tail - head == len) return false;

			buffer[tail % len] = object;
            tail++;
		}

		cv_not_empty.notify_one();

		return true;
	}

	obj *claim() {
		std::unique_lock<std::mutex> l(mtx);
		cv_not_empty.wait(l, [this] () {
			return stop || head != tail;
		});

		if (stop && head == tail) return nullptr;

		obj *object = buffer[head % len];
		head++;

		l.unlock();
		cv_not_full.notify_one();

		return object;
	}

	obj *try_claim() {
		obj *object;

		{
			std::lock_guard<std::mutex> l(mtx);
			if (tail == head) return nullptr;

			object = buffer[head % len];
			head++;
		}

		cv_not_full.notify_one();
		return object;
	}


};



template<class obj, uint32_t len, pool_type type>
requires(type == pool_type::vyukov_spin || type == pool_type::vyukov_idle)
struct mpmc<obj, len, type> {
	struct alignas(64) slot {
		obj *object = nullptr;
		std::atomic<uint32_t> seq_num = 0;
	};

	slot object_buffer[len];

	alignas(64) std::atomic<uint32_t> head, tail;
	std::atomic<bool> stop;

	void shutdown() {
		stop = true;
		tail.notify_all();
	}

	mpmc() {
		for (uint32_t i = 0; i < len; i++) object_buffer[i].seq_num = i;
	}

	~mpmc() { assert(stop); }

	// Blocking method that submits a object to the pool, and waits if it is full.
	// Callers are responsible for keeping `object` alive. Stack allocating is not recommended.
	bool submit(obj *object) {
		for (;;) {
			if (stop.load(std::memory_order_relaxed)) return false;

			auto head_local = head.load(std::memory_order_relaxed),
				 tail_local = tail.load(std::memory_order_relaxed);

			while (tail_local - head_local < len &&
					!tail.compare_exchange_weak(tail_local, tail_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
				head_local = head.load(std::memory_order_relaxed);
			}

			if (tail_local - head_local >= len) {
				head.wait(head_local, std::memory_order_relaxed); continue;
			}

			tail.notify_one();
			
			slot *s = &object_buffer[tail_local % len];
			uint32_t seq_num_local;
			while ((seq_num_local = s->seq_num.load(std::memory_order_acquire))  != tail_local) {
				if constexpr (type == pool_type::vyukov_idle) s->seq_num.wait(seq_num_local, std::memory_order_relaxed);
			}

			s->object = object;
			s->seq_num.store(tail_local + 1, std::memory_order_release);
			if constexpr (type == pool_type::vyukov_idle) s->seq_num.notify_all();

			return true;
		}
	}
	
    // One-shot version of `submit()`. Returns false instead of waiting if pool is full.
	bool try_submit(obj *object) { 
		if (stop.load(std::memory_order_relaxed)) return false;
		
		auto head_local = head.load(std::memory_order_relaxed),
			 tail_local = tail.load(std::memory_order_relaxed);

		while (tail_local - head_local < len &&
				!tail.compare_exchange_weak(tail_local, tail_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
			head_local = head.load(std::memory_order_relaxed);
		}

		if (tail_local - head_local >= len) return false;

		tail.notify_one();

		slot *s = &object_buffer[tail_local % len];
		uint32_t seq_num_local;
		while ((seq_num_local = s->seq_num.load(std::memory_order_acquire))  != tail_local) {
			if constexpr (type == pool_type::vyukov_idle) s->seq_num.wait(seq_num_local, std::memory_order_relaxed);
		}

		s->object = object;
		s->seq_num.store(tail_local + 1, std::memory_order_release);
		if constexpr (type == pool_type::vyukov_idle) s->seq_num.notify_all();

		return true;
	}

    
    // Claims a object from the pool and executes it. Returns whether or not a object was completed
	obj *claim() {
		for (;;) {	
			auto head_local = head.load(std::memory_order_relaxed),
				 tail_local = tail.load(std::memory_order_relaxed);

			while (head_local != tail_local &&
					!head.compare_exchange_weak(head_local, head_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
				tail_local = tail.load(std::memory_order_relaxed);
			}

			if (tail_local == head_local) {
				if (stop.load(std::memory_order_relaxed)) return nullptr;
				tail.wait(tail_local, std::memory_order_relaxed); continue;
			}

			head.notify_one();

			slot *s = &object_buffer[head_local % len];
			uint32_t seq_num_local;

			while ((seq_num_local = s->seq_num.load(std::memory_order_acquire)) != head_local + 1) {
				if constexpr (type == pool_type::vyukov_idle) s->seq_num.wait(seq_num_local, std::memory_order_relaxed);
			}

			obj *object = s->object;
			s->seq_num.store(seq_num_local + len - 1, std::memory_order_release);
			if constexpr (type == pool_type::vyukov_idle) s->seq_num.notify_all();

			return object;
		}
	}
    
    // One-shot version of `claim()`. Returns false instead of waiting.
	obj *try_claim() {
		auto head_local = head.load(std::memory_order_relaxed),
             tail_local = tail.load(std::memory_order_relaxed);

		while (head_local != tail_local
				&& !head.compare_exchange_weak(head_local, head_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
            tail_local = tail.load(std::memory_order_relaxed);
        }

		if (tail_local == head_local) return nullptr;

		head.notify_one();

		slot *s = &object_buffer[head_local % len];
		uint32_t seq_num_local;

		while ((seq_num_local = s->seq_num.load(std::memory_order_acquire)) != head_local + 1) {
			if constexpr (type == pool_type::vyukov_idle) s->seq_num.wait(seq_num_local, std::memory_order_relaxed);
		}

		obj *object = s->object;
		s->seq_num.store(seq_num_local + len - 1, std::memory_order_release);
		if constexpr (type == pool_type::vyukov_idle) s->seq_num.notify_all();

		return object;
	}

	void worker_loop() {
		while (claim());
	}
};
