#ifndef QT_MULTITHREADING_WORKERCONTROLLER_H
#define QT_MULTITHREADING_WORKERCONTROLLER_H

#include <QObject>
#include <QUuid>
#include <QCoreApplication>
#include <QThread>
#include <memory>
#include <cstddef>
#include <deque>
#include <set>
#include <tuple>
#include <vector>
#include "Magic.h"
#include "Worker.h"
#include "Processor.h"

/*
 * Forward declaration of the abstract template class used to control everything. \n
 * Needed, since we are going to make this class a friend.
 *
 * For a full description of this abstract template class see Controller
 */
template<typename T, typename R>
class Controller;

/**
 * This class is used to coordinate everything between the Worker and the Processor instances. \n
 * The class is ready to use as is, nothing has to be implemented by the user of the framework.
 *
 * @tparam T The data type of the task to fulfill
 * @tparam R The data type of the result calculated during fulfilling the task
 */
template<typename T, typename R>
class WorkerController : public QObject {
public:
    /**
     * Removes connections and stops all threads
     */
    ~WorkerController<T, R>() override {
        // mark, that we entered the destructor
        this->isInDestructor = true;

        // disconnect everything
        invokeInContext(
                static_cast<QObject *>(this->processor), Qt::QueuedConnection,
                [processor = this->processor]() -> void { processor->setupConnections(); }
        );
        for (auto &threadTuple : this->threads) {
            invokeInContext(
                    static_cast<QObject *>(std::get<1>(threadTuple)), Qt::BlockingQueuedConnection,
                    [worker = std::get<1>(threadTuple)]() -> void { worker->setupConnections(); }
            );
        }

        // stop workers and wait for them to finish
        this->setNumberOfThreads(0);

        // stop processor
        this->processorThread.quit();

        // the Processor is using BlockingQueuedConnection to connect to this WorkerController
        // thus process all pending events until the Processor actually finished
        while (!this->processorThread.wait(1)) {
            QCoreApplication::processEvents();
        }
    }

private:
    /**
     * Constructor used to initialize the WorkerController.
     *
     * @param prototypeWorker The prototype worker, which will be cloned as many times as specified with \p numberOfThreads
     * @param processor The Processor which will send tasks to the WorkerController and receive results from the Worker
     * @param numberOfThreads The number of threads to use, which is going to be equal to the number of Worker
     */
    WorkerController<T, R>(std::unique_ptr<Worker<T, R>> prototypeWorker, std::unique_ptr<Processor<T, R>> processor,
                           const std::size_t numberOfThreads)
            : QObject(nullptr), prototypeWorker(std::move(prototypeWorker)), processor(processor.get()) {
        // setup the processor
        this->processor->moveToThread(&this->processorThread);
        this->processor->setupConnections(
                static_cast<QObject *>(this), this,
                &WorkerController<T, R>::setNumberOfThreads,
                &WorkerController<T, R>::clearQueue,
                &WorkerController<T, R>::extendQueue
        );
        // start thread and connect deleteLater
        this->processorThread.start();
        QObject::connect(&this->processorThread, &QThread::finished, processor.release(), &QObject::deleteLater);

        // setup threads/workers
        this->setNumberOfThreads(numberOfThreads);
    }

    /**
     * Is changing the number of threads, which implies the number of Worker to be used.
     *
     * @param numberOfThreads The number of threads to use
     */
    inline void setNumberOfThreads(const std::size_t &numberOfThreads) {
        const std::size_t currentNumberOfThreads = this->threads.size();

        // special case setting number of threads to zero
        // blindly stop, remove and reset everything
        if (numberOfThreads == 0) {
            for (auto &threadTuple : this->threads) {
                std::get<0>(threadTuple)->quit();
            }
            for (auto &threadTuple : this->threads) {
                std::get<0>(threadTuple)->wait();
            }
            this->threads.clear();
            this->threads.shrink_to_fit();
            this->workersReady.clear();
        } else if (numberOfThreads < currentNumberOfThreads) {
            for (std::size_t currentThreadIndex = currentNumberOfThreads - 1;
                 currentThreadIndex >= numberOfThreads; --currentThreadIndex) {
                // stop threads, which also kills the workers
                std::get<0>(this->threads[currentThreadIndex])->quit();
                std::get<0>(this->threads[currentThreadIndex])->wait();

                // remove thread from vector and set
                this->threads.pop_back();
                this->workersReady.erase(currentThreadIndex);
            }
            // we are not wasting memory!!!
            this->threads.shrink_to_fit();
        } else if (!this->isInDestructor && numberOfThreads > currentNumberOfThreads) {
            for (std::size_t currentThreadIndex = currentNumberOfThreads;
                 currentThreadIndex < numberOfThreads; ++currentThreadIndex) {
                // clone prototype worker
                std::unique_ptr<Worker<T, R>> newWorker = this->prototypeWorker->clone();

                // create new thread for the worker
                this->threads.emplace_back(
                        std::make_unique<QThread>(), newWorker.get(), QUuid(newWorker->uniqueWorkerUUID)
                );
                this->workersReady.insert(currentThreadIndex);

                // setup connections for new worker
                QThread &newThread = *std::get<0>(this->threads.back());
                newWorker->moveToThread(&newThread);
                newWorker->setupConnections(
                        currentThreadIndex + 1,
                        static_cast<QObject *>(this), this, static_cast<QObject *>(this->processor), this->processor,
                        &WorkerController<T, R>::workerFinished,
                        &Processor<T, R>::receiveResult
                );

                // start thread and connect deleteLater
                newThread.start();
                QObject::connect(&newThread, &QThread::finished, newWorker.release(), &QObject::deleteLater);
            }

            // check if new tasks may be given to workers
            this->checkTasks();
        }
    }

    /**
     * Clears the queue containing the tasks to be sent to Worker
     */
    inline void clearQueue() {
        if (!this->isInDestructor) {
            this->tasks.clear();
            this->tasks.shrink_to_fit();
        }
    }

    /**
     * Extends the queue with tasks for Worker with new tasks
     *
     * @param newTasks The queue containing the new tasks
     */
    inline void extendQueue(const std::deque<T> &newTasks) {
        if (!this->isInDestructor) {
            this->tasks.insert(this->tasks.cend(), newTasks.begin(), newTasks.end());
            checkTasks();
        }
    }

    /**
     * Checks if new tasks have to be given to the Worker, and does it, if needed.
     */
    inline void checkTasks() {
        for (auto threadIndex = this->workersReady.begin();
             !this->tasks.empty() && threadIndex != this->workersReady.end();) {
            // send new task to worker
            invokeInContext(
                    static_cast<QObject *>(std::get<1>(this->threads[*threadIndex])), Qt::QueuedConnection,
                    &Worker<T, R>::receiveTask,
                    std::get<1>(this->threads[*threadIndex]), this->tasks.front()
            );
            // remove task from queue
            this->tasks.pop_front();
            // mark worker as fulfilling a task
            threadIndex = this->workersReady.erase(threadIndex);
        }
    }

    /**
     * Received, when a Worker has finished working
     * @param workerID The ID of the Worker finished working, which is the same as the index in this->threads + 1
     */
    inline void workerFinished(const std::size_t &workerID, const QUuid &workerUUID) {
        const std::size_t threadIndex = workerID - 1;

        if (threadIndex < this->threads.size() && workerUUID == std::get<2>(this->threads[threadIndex])) {
            this->workersReady.insert(threadIndex);
            checkTasks();
        }
    }

    /**
     * The prototype Worker which is going to be cloned to create the real Worker
     */
    std::unique_ptr<Worker<T, R>> prototypeWorker;
    /**
     * The Processor which is going to send tasks to the WorkerController and receive results from the Worker
     */
    Processor<T, R> *processor;
    /**
     * The thread containing the Processor instance
     */
    QThread processorThread;
    /**
     * std::vector containing the threads, the corresponding pointers to the Worker
     * and a unique UUID to identify the Worker
     */
    std::vector<std::tuple<std::unique_ptr<QThread>, Worker<T, R> *, QUuid>> threads;
    /**
     * std::deque containing the tasks going to be sent to Worker
     */
    std::deque<T> tasks;
    /**
     * std::set containing the indices (referring to WorkerController::threads) of Worker
     * currently not fulfilling a task
     */
    std::set<std::size_t> workersReady;
    /**
     * Used to not allow the creation of new Worker when already in the destructor
     */
    bool isInDestructor = false;

    /**
     * In order to be able to instantiate this class, whose constructor is private, making the
     * template class Controller a friend is necessary.
     */
    friend class Controller<T, R>;
};

#endif
