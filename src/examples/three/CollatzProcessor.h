#ifndef QT_MULTITHREADING_COLLATZPROCESSOR_H
#define QT_MULTITHREADING_COLLATZPROCESSOR_H

#include <QWaitCondition>
#include <chrono>
#include <tuple>
#include "../../Processor.h"

using time_point = std::chrono::time_point<std::chrono::high_resolution_clock>;
using return_tuple = std::tuple<unsigned int, unsigned int, time_point>;
using result_tuple = std::tuple<unsigned int, unsigned int, time_point, time_point>;

/**
 * Processor which does not directly process the results and emits them via a Signal instead, unless wanted
 */
class CollatzProcessor : public Processor<unsigned int, return_tuple> {
public:
    /**
     * Constructor for the Processor
     *
     * @param qWaitCondition A QWaitCondition used to wake up threads when all tasks are done
     */
    explicit CollatzProcessor(QWaitCondition &qWaitCondition) : qWaitCondition(qWaitCondition) {}

    /**
     * Allows to set the number of threads to use
     *
     * @param numberOfThreads The number of threads to use
     */
    inline void setNumberThreads(const std::size_t &numberOfThreads) {
        // already implemented protected method of the abstract base class
        this->setNumberOfThreads(numberOfThreads);
    }

    /**
     * Clears the std::deque containing the tasks to fulfill
     */
    inline void clearTasks() {
        this->toProcess = 0;
        // already implemented protected method of the abstract base class
        this->clearQueue();
    }

    /**
     * Appends new tasks to the std::deque containing the tasks to fulfill
     *
     * @param newTasks The new tasks to fulfill
     */
    inline void extendTasks(const std::deque<unsigned int> &newTasks) {
        this->toProcess += newTasks.size();
        // already implemented protected method of the abstract base class
        this->extendQueue(newTasks);
    }

    /**
     * See CollatzProcessor::processHere
     *
     * @param processHereB See CollatzProcessor::processHere
     */
    inline void toProcessHere(bool processHereB) {
        this->processHere = processHereB;
    }

    /**
     * Prints a result
     *
     * @param result The result to print
     */
    static inline void printResult(const result_tuple &result) {
        auto signalDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::get<3>(result) - std::get<2>(result)
        ).count();

        std::cout << "Total stopping time of " << std::get<0>(result) << ": " << std::get<1>(result)
                  << " - with signal transmission time: " << signalDuration << " microseconds" << std::endl;
    }

    /**
     * This signal is going to emit received results
     */
    Signal<result_tuple> resultSignal;
private:
    /**
     * The number of tasks, which have yet to be fulfilled. \n
     * When it reaches 0, .wakeAll() is called on CollatzProcessor::qWaitCondition
     */
    std::size_t toProcess = 0;
    /**
     * The QWaitCondition used to wake up threads
     */
    QWaitCondition &qWaitCondition;
    /**
     * If set to \p false, the results are being emitted with the Signal,
     * if set to \p true, the results are being processed in the CollatzProcessor itself
     */
    bool processHere = false;

    /**
     * Called when a worker has calculated a result. \n
     * Appends the timestamp of having received the result.
     *
     * Wakes up threads with calling .wakeAll() on CollatzProcessor::qWaitCondition when all tasks have been fulfilled
     *
     * @param result    A std::tuple containing the number of which the total stopping time has been calculated,
     *                  the calculated total stopping time and the timestamp at which the CollatzWorker sent the result
     */
    inline void receiveResult(return_tuple &result) override {
        auto resultTuple = std::make_tuple(
                std::get<0>(result), std::get<1>(result), std::get<2>(result),
                std::chrono::high_resolution_clock::now()
        );

        if (!processHere) {
            this->resultSignal(resultTuple);
        } else {
            CollatzProcessor::printResult(resultTuple);
        }

        if (this->toProcess && !(--this->toProcess)) {
            qWaitCondition.wakeAll();
        }
    }
};

#endif
