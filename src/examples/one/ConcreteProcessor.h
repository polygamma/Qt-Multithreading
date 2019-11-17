#ifndef QT_MULTITHREADING_CONCRETEPROCESSOR_H
#define QT_MULTITHREADING_CONCRETEPROCESSOR_H

#include <iostream>
#include <QWaitCondition>
#include "../../Processor.h"

/**
 * See https://doc.qt.io/qt-5/qmetatype.html#Q_DECLARE_SEQUENTIAL_CONTAINER_METATYPE
 * @tparam T std::deque
 */
Q_DECLARE_SEQUENTIAL_CONTAINER_METATYPE (std::deque)

/**
 * A processor able to give tasks to Worker<int, int> and receive the results,
 * wake up threads when all tasks are completed
 * and allows the usage of all protected methods of Processor<int, int> via Qt Slots
 */
class ConcreteProcessor : public Processor<int, int> {
Q_OBJECT
public:
    /**
     * Constructor used to init the ConcreteProcessor
     *
     * @param qWaitCondition See ConcreteProcessor::qWaitCondition
     */
    explicit ConcreteProcessor(QWaitCondition &qWaitCondition) : qWaitCondition(qWaitCondition) {
        /**
         * We are using Qt Signals and Slots, thus register everything needed
         */
        qRegisterMetaType<std::deque<int>>();
        qRegisterMetaType<std::size_t>();
    }

public slots:

    /**
     * Sets a new message to be printed in front of every result
     *
     * @param newToPrint The new message to be prepended to the received results
     */
    inline void setNewMessageToPrint(const std::string &newToPrint) {
        this->toPrint = newToPrint;
    }

    /**
     * Starts working on new tasks
     *
     * @param newTasks The new tasks to fulfill
     */
    inline void giveNewTasks(const std::deque<int> &newTasks) {
        this->toProcess += newTasks.size();
        // makes use of the already implemented protected method of the abstract base class
        this->extendQueue(newTasks);
    }

    /**
     * Resets all tasks to fulfill
     */
    inline void clearTasks() {
        this->toProcess = 0;
        // makes use of the already implemented protected method of the abstract base class
        this->clearQueue();
    }

    /**
     * Sets the number of threads, which is the same as the number of workers
     *
     * @param numberThreads The number of threads/workers to use
     */
    inline void setThreads(std::size_t numberThreads) {
        // makes use of the already implemented protected method of the abstract base class
        this->setNumberOfThreads(numberThreads);
    }

private:
    /**
     * A message going to be printed in front of every result
     */
    std::string toPrint = "default message";
    /**
     * Being used to wake up threads when all tasks are completed
     */
    QWaitCondition &qWaitCondition;
    /**
     * Tracks the number of tasks to fulfill
     */
    std::size_t toProcess = 0;

    /**
     * Prints the received result with a prepended message
     *
     * @param result The result received from a worker
     */
    inline void receiveResult(int &result) override {
        std::cout << this->toPrint << ": " << result << std::endl;
        // wake up when everything is done
        if (this->toProcess && !(--this->toProcess)) {
            this->qWaitCondition.wakeAll();
        }
    }
};

#endif
