#ifndef QT_MULTITHREADING_PROCESSOR_H
#define QT_MULTITHREADING_PROCESSOR_H

#include <QObject>
#include <deque>
#include <cstddef>
#include "Magic.h"

/*
 * Forward declaration of the template class used to coordinate everything.
 * Sets up the needed connections between workers and processors, forwards tasks to the workers etc. \n
 * Needed, since we are going to store pointers to objects of this type, but a definition is not needed,
 * since we are really only doing that, nothing else.
 *
 * For a full description of this template class see WorkerController
 */
template<typename T, typename R>
class WorkerController;

/**
 * Abstract template class to provide the skeleton for the concrete Processor of this framework. \n
 * The user of the framework has to inherit from this class and implement at least one function, namely receiveResult()
 * which is being called when a Worker has fulfilled a given task. The function is going to be called with the result
 * as parameter.
 *
 * Pretty easy, isn't it?
 *
 * In reality, only implementing that function is not going to be enough, since in that case new tasks would never
 * be given to the WorkerController. \n
 * Three more protected methods, which are already implemented, setNumberOfThreads(), clearQueue() and extendQueue()
 * can be called at will, to communicate with the WorkerController.
 *
 * Notice: Those functions may ONLY be called from within this Processor, so that a separate communication channel with
 * the Processor is almost likely needed and can e. g. be established with Qt Signals and Slots. But be aware of the fact,
 * that the Processor is also going to be run in a new thread, which implies either a thread safe implementation of
 * the functions to be called via that new communication channel
 * or using e. g. Qt::QueuedConnection or Qt::BlockingQueuedConnection
 * to never access the functions from more than one thread (the thread in which the Processor is running) at once.
 *
 * @tparam T The data type of the task to fulfill
 * @tparam R The data type of the result calculated during fulfilling the task
 */
template<typename T, typename R>
class Processor : public SlotProvider {
protected:
    /**
     * The only constructor provided by this abstract class, which is going to be called by all constructors
     * of inherited classes automatically. This works, because the constructor is a default constructor. \n
     * Initializes the Processor with sane defaults.
     */
    Processor<T, R>()
            : SlotProvider(nullptr), workerControllerContext(nullptr), workerController(nullptr),
              setNumberOfThreadsPtr(nullptr), clearQueuePtr(nullptr), extendQueuePtr(nullptr) {}

    /**
    * See Processor::setNumberOfThreadsPtr
    */
    inline void setNumberOfThreads(const std::size_t &numberOfThreads) {
        invokeInContext(
                this->workerControllerContext, Qt::BlockingQueuedConnection,
                this->setNumberOfThreadsPtr,
                this->workerController, numberOfThreads
        );
    }

    /**
     * See Processor::clearQueuePtr
     */
    inline void clearQueue() {
        invokeInContext(
                this->workerControllerContext, Qt::BlockingQueuedConnection,
                this->clearQueuePtr,
                this->workerController
        );
    }

    /**
     * See Processor::extendQueuePtr
     */
    inline void extendQueue(const std::deque<T> &newTasks) {
        invokeInContext(
                this->workerControllerContext, Qt::BlockingQueuedConnection,
                this->extendQueuePtr,
                this->workerController, newTasks
        );
    }

private:
    /**
     * Method to be implemented when inheriting from this abstract template class. \n
     * This function is going to called when a Worker has finished fulfilling a task and receives that result
     * via the parameter \p result
     *
     * You may want to notice, that this is a private virtual function.
     * See https://isocpp.org/wiki/faq/strange-inheritance#private-virtuals
     *
     * @param result The result which has been calculated during fulfilling of a task
     */
    virtual void receiveResult(R &result) = 0;

    /**
     * This function is going to be called to setup all the needed connections between this Processor and the
     * WorkerController.
     *
     * May be called without arguments to invalidate the pointer to the WorkerController so that
     * signals sent by this Processor are no longer received.
     *
     * @param newWorkerControllerContext See Processor::workerControllerContext
     * @param newWorkerController See Processor::workerController
     * @param newSetNumberOfThreadsPtr See Processor::setNumberOfThreadsPtr
     * @param newClearQueuePtr See Processor::clearQueuePtr
     * @param newExtendQueuePtr See Processor::extendQueuePtr
     */
    inline void
    setupConnections(QObject *const newWorkerControllerContext = nullptr,
                     WorkerController<T, R> *const newWorkerController = nullptr,
                     void (WorkerController<T, R>::* const newSetNumberOfThreadsPtr)(const std::size_t &) = nullptr,
                     void (WorkerController<T, R>::* const newClearQueuePtr)() = nullptr,
                     void (WorkerController<T, R>::* const newExtendQueuePtr)(const std::deque<T> &) = nullptr) {
        this->workerControllerContext = newWorkerControllerContext;
        // only set new ptr to worker controller if not nullptr
        this->workerController = newWorkerController ? newWorkerController : this->workerController;

        // only set new function pointers if they are not nullptr
        this->setNumberOfThreadsPtr = newSetNumberOfThreadsPtr ? newSetNumberOfThreadsPtr : this->setNumberOfThreadsPtr;
        this->clearQueuePtr = newClearQueuePtr ? newClearQueuePtr : this->clearQueuePtr;
        this->extendQueuePtr = newExtendQueuePtr ? newExtendQueuePtr : this->extendQueuePtr;
    }

    /**
     * Pointer to the relevant WorkerController already
     * casted to a QObject*
     */
    QObject *workerControllerContext;

    /**
     * Pointer to the relevant WorkerController
     */
    WorkerController<T, R> *workerController;

    /**
     * Can be called to change the number of threads to be used for the Worker
     *
     * Notice: This function is going to be invoked with a Qt::BlockingQueuedConnection, so that the function call is only
     * going to return after the WorkerController has processed the function call.
     */
    void (WorkerController<T, R>::* setNumberOfThreadsPtr)(const std::size_t &);

    /**
     * Can be called to clear the queue of the WorkerController containing the tasks to be sent to Worker
     *
     * Notice: This function is going to be invoked with a Qt::BlockingQueuedConnection, so that the function call is only
     * going to return after the WorkerController has processed the function call.
     */
    void (WorkerController<T, R>::* clearQueuePtr)();

    /**
     * Can be called to extend the queue of the WorkerController containing the tasks to be sent to Worker
     *
     * Notice: This function is going to be invoked with a Qt::BlockingQueuedConnection, so that the function call is only
     * going to return after the WorkerController has processed the function call.
     */
    void (WorkerController<T, R>::* extendQueuePtr)(const std::deque<T> &);

    /**
     * In order to be able to set up the connections etc. which is only allowed via a private method, making the
     * template class WorkerController a friend is necessary.
     */
    friend class WorkerController<T, R>;
};

#endif
