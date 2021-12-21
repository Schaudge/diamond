#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <condition_variable>

namespace Util { namespace Parallel {

template<typename _f, typename... _args>
void pool_worker(std::atomic<size_t> *partition, size_t thread_id, size_t partition_count, _f f, _args... args) {
	size_t p;
	while ((p = (*partition)++) < partition_count) {
		f(p, thread_id, args...);
	}
}

template<typename _f, typename... _args>
void scheduled_thread_pool(size_t thread_count, _f f, _args... args) {
	std::atomic<size_t> partition(0);
	std::vector<std::thread> threads;
	for (size_t i = 0; i < thread_count; ++i)
		threads.emplace_back(f, &partition, i, args...);
	for (std::thread &t : threads)
		t.join();
}

template<typename _f, typename... _args>
void scheduled_thread_pool_auto(size_t thread_count, size_t partition_count, _f f, _args... args) {
	scheduled_thread_pool(thread_count, pool_worker<_f, _args...>, partition_count, f, args...);
}

}}

struct ThreadPool {

	struct TaskSet {
		TaskSet(ThreadPool& thread_pool):
			total(0),
			finished(0),
			thread_pool(&thread_pool)
		{}
		void finish() {
			++finished;
			cv_.notify_one();
		}
		void add() {
			++total;
		}
		void wait() {
			std::unique_lock<std::mutex> lock(mtx_);
			cv_.wait(lock, [this] {return total == finished; });
		}
		template<class F, class... Args>
		void enqueue(F&& f, Args&&... args) {
			thread_pool->enqueue(*this, std::forward<F>(f), std::forward<Args>(args)...);
		}
	private:
		std::atomic<int64_t> total, finished;
		ThreadPool* thread_pool;
		std::mutex mtx_;
		std::condition_variable cv_;
	};

	struct Task {
		Task() {}
		Task(std::function<void()> f, TaskSet& task_set):
			f(f),
			task_set(&task_set)
		{}
		std::function<void()> f;
		TaskSet* task_set;
	};

	template<class F, class... Args>
	void enqueue(TaskSet& task_set, F&& f, Args&&... args)
	{
		auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
		{
			std::unique_lock<std::mutex> lock(mtx_);

			if (stop_)
				throw std::runtime_error("enqueue on stopped ThreadPool");

			task_set.add();
			tasks_.emplace([task]() { task(); }, task_set);
		}
		cv_.notify_one();
	}

	ThreadPool(size_t threads) :
		stop_(false)
	{
		for (size_t i = 0; i < threads; ++i)
			workers_.emplace_back(
				[this]
		{
			for (;;)
			{
				Task task;

				{
					std::unique_lock<std::mutex> lock(this->mtx_);
					this->cv_.wait(lock,
						[this] { return this->stop_ || !this->tasks_.empty(); });
					if (this->stop_ && this->tasks_.empty())
						return;
					task = std::move(this->tasks_.front());
					this->tasks_.pop();
				}

				task.f();
				task.task_set->finish();
			}
		}
		);
	}

	~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(mtx_);
			stop_ = true;
		}
		cv_.notify_all();
		for (std::thread &worker : workers_)
			worker.join();
	}

private:

	std::queue<Task> tasks_;
	std::vector<std::thread> workers_;
	std::mutex mtx_;
	std::condition_variable cv_;
	bool stop_;

};
