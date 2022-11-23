#ifndef QT_MULTITHREADING_WORKER_H
#define QT_MULTITHREADING_WORKER_H

#include <QObject>
#include <QUuid>
#include <cstddef>
#include <memory>
#include "Magic.h"

/*
 * Forward declaration of the abstract template class used to give tasks and receive the results. \n
 * Needed, since we are going to store pointers to objects of this type, but a definition is not needed,
 * since we are really only doing that, nothing else.
 *
 * For a full description of this abstract template class see Processor
 */
template<typename T, typename R>
class Processor;

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
 * Abstract template class to provide the skeleton for the concrete Worker of this framework. \n
 * The user of the framework has to inherit from this class and implement two functions, namely fulfillTask()
 * which is able to produce a result of type \p R from a given task of type \p T and clone() which clones an existing
 * Worker.
 *
 * Pretty easy, isn't it?
 *
 * @tparam T The data type of the task to fulfill
 * @tparam R The data type of the result calculated during fulfilling the task
 */
template<typename T, typename R>
class Worker : public QObject {
public:
    /**
     * Be explicit about the destructor, since we are explicit about the copy constructor,
     * move constructor, copy assignment operator and move assignment operator, too
     */
    ~Worker() override = default;

protected:
    /**
     * The only constructor provided by this abstract class, which is going to be called by all constructors
     * of inherited classes automatically. This works, because the constructor is a default constructor. \n
     * Initializes the workers with sane defaults.
     */
    Worker()
            : QObject(nullptr), workerUUID(0), uniqueWorkerUUID(QUuid::createUuid()),
              workerControllerContext(nullptr), workerController(nullptr),
              processorContext(nullptr), processor(nullptr),
              workDone(nullptr), resultCalculated(nullptr) {}

    /**
     * Be explicit about not wanting a copy constructor
     */
    Worker(const Worker<T, R> &) = delete;

    /**
     * Be explicit about not wanting a move constructor
     */
    Worker(Worker<T, R> &&) = delete;

    /**
     * Be explicit about not wanting a copy assignment operator
     */
    Worker<T, R> &operator=(const Worker<T, R> &) = delete;

    /**
     * Be explicit about not wanting a move assignment operator
     */
    Worker<T, R> &operator=(Worker<T, R> &&) = delete;

private:
    /**
     * Method to be implemented when inheriting from this abstract template class. \n
     * Is going to contain the logic to handle the given task.
     *
     * You may want to notice, that this is a private virtual function.
     * See https://isocpp.org/wiki/faq/strange-inheritance#private-virtuals
     *
     * @param task The task to fulfill
     * @return The result calculated during fulfilling the task
     */
    virtual R fulfillTask(T &task) = 0;

    /**
     * This method is used to clone an existing Worker. \n
     * Has to be implemented when inheriting from this abstract template class by the user of the framework,
     * so that a cloned Worker is able to fulfill tasks just like the Worker which is being cloned.
     *
     * This includes ONLY things defined by the user of the framework, no internal connections like the pointers
     * to the WorkerController and the Processor. The user would not be able to do that anyway, since those variables
     * are private.
     *
     * You may want to notice, that this method implements the "virtual constructor idiom". \n
     * See https://isocpp.org/wiki/faq/virtual-functions#virtual-ctors
     * and https://www.fluentcpp.com/2017/09/08/make-polymorphic-copy-modern-cpp/
     *
     * You may want to notice, that this is a private virtual function.
     * See https://isocpp.org/wiki/faq/strange-inheritance#private-virtuals
     *
     * @return The cloned Worker
     */
    virtual std::unique_ptr<Worker<T, R>> clone() = 0;

    /**
     * Is going to be connected to the WorkerController so that this Worker will receive new tasks to fulfill
     * via this method
     *
     * @param task The new task to fulfill
     */
    inline void receiveTask(T &task) {
        // fulfill task and receive result
        R result = fulfillTask(task);
        // send result to the Processor
        invokeInContext(
                this->processorContext, Qt::QueuedConnection, this->resultCalculated,
                this->processor, result
        );
        // notify the WorkerController about being ready for a new task
        invokeInContext(
                this->workerControllerContext, Qt::QueuedConnection, this->workDone,
                this->workerController, this->workerUUID, this->uniqueWorkerUUID
        );
    }

    /**
     * This function is going to be called to setup all the needed connections between this Worker and the
     * WorkerController and the Processor.
     *
     * May be called without arguments to invalidate the pointers to the WorkerController and the Processor so that
     * signals sent by this Worker are no longer received.
     *
     * @param newWorkerUUID See Worker::workerUUID
     * @param newWorkerControllerContext See Worker::workerControllerContext
     * @param newWorkerController See Worker::workerController
     * @param newProcessorContext See Worker::processorContext
     * @param newProcessor See Worker::processor
     * @param newWorkDone See Worker::workDone
     * @param newResultCalculated See Worker::resultCalculated
     */
    inline void
    setupConnections(const std::size_t newWorkerUUID = 0,
                     QObject *const newWorkerControllerContext = nullptr,
                     WorkerController<T, R> *const newWorkerController = nullptr,
                     QObject *const newProcessorContext = nullptr,
                     Processor<T, R> *const newProcessor = nullptr,
                     void (WorkerController<T, R>::* const newWorkDone)(const std::size_t &, const QUuid &) = nullptr,
                     void (Processor<T, R>::* const newResultCalculated)(R &) = nullptr) {
        this->workerControllerContext = newWorkerControllerContext;
        this->processorContext = newProcessorContext;

        // only set new workerUUID if it is not 0
        this->workerUUID = newWorkerUUID ? newWorkerUUID : this->workerUUID;

        // only set new ptr to worker controller if not nullptr
        this->workerController = newWorkerController ? newWorkerController : this->workerController;

        // only set new ptr to processor if not nullptr
        this->processor = newProcessor ? newProcessor : this->processor;

        // only set new function pointers if they are not nullptr
        this->workDone = newWorkDone ? newWorkDone : this->workDone;
        this->resultCalculated = newResultCalculated ? newResultCalculated : this->resultCalculated;
    }

    /**
     * UUID used to identify Worker. \n
     * Is going to be sent back to the WorkerController after having fulfilled a task,
     * so that the WorkerController knows which Worker are ready to receive new tasks.
     */
    std::size_t workerUUID;

    /**
     * Unique UUID used to identify Worker. \n
     * Is going to be sent back to the WorkerController after having fulfilled a task,
     * so that the WorkerController knows which Worker are ready to receive new tasks.
     */
    const QUuid uniqueWorkerUUID;

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
     * Pointer to the relevant Processor already
     * casted to a QObject*
     */
    QObject *processorContext;

    /**
     * Pointer to the relevant Processor
     */
    Processor<T, R> *processor;

    /**
     * Going to be invoked in the thread of the WorkerController after having fulfilled a task to notify the
     * WorkerController about being ready to receive a new task.
     */
    void (WorkerController<T, R>::* workDone)(const std::size_t &, const QUuid &);

    /**
     * Going to be invoked in the thread of the Processor after having fulfilled a task to send the result
     * to the Processor.
     */
    void (Processor<T, R>::* resultCalculated)(R &);

    /**
     * In order to be able to set up the connections etc. which is only allowed via a private method, making the
     * template class WorkerController a friend is necessary.
     */
    friend class WorkerController<T, R>;
};

#endif
