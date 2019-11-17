#ifndef QT_MULTITHREADING_COLLATZWORKER_H
#define QT_MULTITHREADING_COLLATZWORKER_H

#include "../../Worker.h"
#include "CollatzProcessor.h"

/**
 * See: https://en.wikipedia.org/wiki/Collatz_conjecture
 *
 * The worker is able to calculate the total stopping time of a given number.
 */
class CollatzWorker : public Worker<unsigned int, return_tuple> {
private:
    /**
     * Calculates the total stopping time of \p task
     *
     * @param task The number to calculate the total stopping time of
     * @return  A std::tuple containing \p task, the total stopping time of \p task and a timestamp created
     *          when returning.
     */
    inline return_tuple fulfillTask(unsigned int &task) override {
        unsigned int sequenceLength = 1, n = task;
        while (n > 1) {
            if (n % 2) {
                n *= 3;
                n += 1;
            } else {
                n /= 2;
            }
            ++sequenceLength;
        }
        return std::make_tuple(task, sequenceLength, std::chrono::high_resolution_clock::now());
    }

    /**
     * Clones the worker.
     *
     * @return The cloned worker
     */
    inline std::unique_ptr<Worker<unsigned int, return_tuple>> clone() override {
        return std::make_unique<CollatzWorker>();
    }
};

#endif
