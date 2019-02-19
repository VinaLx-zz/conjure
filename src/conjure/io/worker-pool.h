#ifndef CONJURE_IO_WORKER_POOL_H_
#define CONJURE_IO_WORKER_POOL_H_

#include "conjure/io/job.h"
#include "conjure/io/sync-queue.h"
#include "conjure/log.h"
#include <chrono>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

namespace conjure::io {

class Worker {
  public:
    using Queue = SyncQueue<PrimJob>;
    using Pointer = std::unique_ptr<Worker>;

    Worker(int id, int queue_size = Queue::kDefaultCapacity)
        : id_(id), queue_(queue_size) {}

    template <typename JobImpl, typename R>
    bool Submit(Job<JobImpl, R> &job) {
        return queue_.UnsyncPush(job.ToPrimitive());
    }

    bool Available() const {
        return not queue_.Full();
    }

    int PendingJob() const {
        return queue_.Size();
    }

    void Stop() {
        should_stop_ = true;
        thread_.join();
        CONJURE_LOGF("worker %d stopped", id_);
    }

    void Start() {
        if (thread_.joinable()) {
            // already started
            return;
        }
        thread_ = std::thread([this]() { ProcessJobs(); });
    }

  private:
    void ProcessJobs() {
        for (;;) {
            if (should_stop_) {
                break;
            }
            PrimJob j;
            if (TryGetJob(j)) {
                j.Call();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    bool TryGetJob(PrimJob &j) {
        if (not queue_.Empty()) {
            queue_.UnsyncPop(j);
            return true;
        }
        return false;
    }

    [[maybe_unused]] int id_;
    Queue queue_;
    std::thread thread_;
    volatile bool should_stop_ = false;
};

class WorkerPool {
  public:
    WorkerPool() = default;
    WorkerPool(int pool_size) {
        for (int i = 0; i < pool_size; ++i) {
            AddWorker();
        }
        StartAll();
    }

    ~WorkerPool() {
        StopAll();
    }

    static WorkerPool &Instance(int pool_size = 8) {
        static WorkerPool worker_pool(pool_size);
        return worker_pool;
    }

    int Size() const {
        return workers_.size();
    }

    void AddWorker() {
        workers_.push_back(std::make_unique<Worker>(Size()));
    }

    Worker &GetWorker(int n) {
        return *workers_.at(n);
    }

    bool StartAll() {
        if (active_) {
            return false;
        }
        for (auto &worker : workers_) {
            worker->Start();
        }
        active_ = true;
        return true;
    }

    bool StopAll() {
        CONJURE_LOGL("worker pool stopping");
        if (not active_) {
            return false;
        }
        for (auto &worker : workers_) {
            worker->Stop();
        }
        return true;
    }

    template <typename JobImpl, typename R>
    void Submit(Job<JobImpl, R> &job) {
        int best_idx = 0;
        int least_jobs = std::numeric_limits<int>::max();
        for (int i = 0; i < workers_.size(); ++i) {
            int n = workers_[i]->PendingJob();
            if (n < least_jobs) {
                least_jobs = n;
                best_idx = i;
            }
        }
        workers_[best_idx]->Submit(job);
    }

  private:
    std::vector<Worker::Pointer> workers_;
    bool active_ = false;
};

} // namespace conjure::io

#endif // CONJURE_IO_WORKER_POOL_H_
