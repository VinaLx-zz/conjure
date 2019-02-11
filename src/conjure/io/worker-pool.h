#ifndef CONJURE_IO_WORKER_POOL_H_
#define CONJURE_IO_WORKER_POOL_H_

#include "conjure/io/job.h"
#include "conjure/io/sync-queue.h"
#include <chrono>
#include <thread>
#include <vector>

namespace conjure::io {

class Worker {
  public:
    using Queue = SyncQueue<PrimJob>;

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
    }

    void Start() {
        // TODO
        thread_ = std::thread([this]() {
            for (;;) {
                if (should_stop_) {
                    break;
                }
                if (not queue_.Empty()) {
                    PrimJob j;
                    queue_.UnsyncPop(j);
                    j.Call();
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
    }

  private:
    Queue queue_;
    std::thread thread_;
    volatile bool should_stop_ = false;
};

class WorkerPool {
  public:
    WorkerPool() = default;
    WorkerPool(int pool_size) : workers_(pool_size) {
        for (auto &worker : workers_) {
            worker.Start();
        }
    }

    ~WorkerPool() {
        for (auto &worker : workers_) {
            worker.Stop();
        }
    }

    static WorkerPool &Instance(int pool_size = 8) {
        static WorkerPool worker_pool(pool_size);
        return worker_pool;
    }

    template <typename JobImpl, typename R>
    void Submit(Job<JobImpl, R> &job) {
        int best_idx = 0;
        int least_jobs = std::numeric_limits<int>::max();
        for (int i = 0; i < workers_.size(); ++i) {
            int n = workers_[i].PendingJob();
            if (n < least_jobs) {
                least_jobs = n;
                best_idx = i;
            }
        }
        workers_[best_idx].Submit(job);
    }

  private:
    std::vector<Worker> workers_;
};

} // namespace conjure::io

#endif // CONJURE_IO_WORKER_POOL_H_
