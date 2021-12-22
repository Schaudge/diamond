#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <condition_variable>
#include <numeric>

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

	enum { PRIORITY_COUNT = 2 };

	struct TaskSet {
		TaskSet(ThreadPool& thread_pool, int priority):
			priority(priority),
			total_(0),
			finished_(0),
			thread_pool(&thread_pool)
		{}
		void finish() {
			++finished_;
			if (finished()) {
				cv_.notify_all();
				thread_pool->cv_.notify_all();
			}
		}
		bool finished() const {
			return total_ == finished_;
		}
		int64_t total() const {
			return total_;
		}
		void add() {
			++total_;
		}
		void wait() {
			std::unique_lock<std::mutex> lock(mtx_);
			cv_.wait(lock, [this] {return this->finished(); });
		}
		void run() {
			if (finished())
				return;
			thread_pool->run_set(this);
		}
		template<class F, class... Args>
		void enqueue(F&& f, Args&&... args) {
			thread_pool->enqueue(*this, std::forward<F>(f), std::forward<Args>(args)...);
		}
	private:
		const int priority;
		std::atomic<int64_t> total_, finished_;
		ThreadPool* thread_pool;
		std::mutex mtx_;
		std::condition_variable cv_;
		friend struct ThreadPool;
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
			tasks_[task_set.priority].emplace([task]() { task(); }, task_set);
		}
		cv_.notify_one();
	}

	void run_set(TaskSet* task_set) {
		for (;;)
		{
			Task task;

			{
				std::unique_lock<std::mutex> lock(this->mtx_);
				this->cv_.wait(lock,
					[this, task_set] { return this->stop_ || !queue_empty() || (task_set && task_set->finished()); });
				if ((this->stop_ && queue_empty()) || (task_set && task_set->finished()))
					return;
				for(auto& q : tasks_)
					if (!q.empty()) {
						task = std::move(q.front());
						q.pop();
						break;
					}
			}

			task.f();
			task.task_set->finish();
		}
	}

	ThreadPool(size_t threads) :
		stop_(false)
	{
		for (size_t i = 0; i < threads; ++i)
			workers_.emplace_back([this] {	this->run_set(nullptr); });
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

	int64_t queue_len(int priority) const {
		return tasks_[priority].size();
	}

private:

	bool queue_empty() {
		for (auto& q : tasks_)
			if (!q.empty())
				return false;
		return true;
	}

	std::array<std::queue<Task>, PRIORITY_COUNT> tasks_;
	std::vector<std::thread> workers_;
	std::mutex mtx_;
	std::condition_variable cv_;
	bool stop_;

};
