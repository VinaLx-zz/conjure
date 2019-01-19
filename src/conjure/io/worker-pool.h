#ifndef CONJURE_IO_WORKER_POOL_H_
#define CONJURE_IO_WORKER_POOL_H_

#include "conjure/io/job.h"
#include "conjure/io/sync-queue.h"
#include <thread>
#include <vector>

namespace conjure::io {

class Worker {
  public:
    using Queue = SyncQueue<PrimJob>;

    template <typename JobImpl>
    bool Submit(const Job<JobImpl> &job) {
        return queue_.UnsyncPush(job.EraseType());
    }

  private:
    Queue queue_;
};

class WorkerPool {

  public:
    WorkerPool(int pool_size) : threads_(pool_size) {}

    template <typename JobImpl>
    void Submit(const Job<JobImpl> &job) {}

  private:
    std::vector<std::thread> threads_;
};

} // namespace conjure::io

#endif // CONJURE_IO_WORKER_POOL_H_
