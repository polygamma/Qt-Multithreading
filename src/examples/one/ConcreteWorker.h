#ifndef QT_MULTITHREADING_CONCRETEWORKER_H
#define QT_MULTITHREADING_CONCRETEWORKER_H

#include <QThread>
#include "../../Worker.h"

/**
 * A worker who is able to fulfill the incredible task of adding one to a given number
 */
class ConcreteWorker : public Worker<int, int> {
private:
    /**
     * Going to fulfill the incredible task of adding one to a given number
     *
     * @param task The number to add one to
     * @return The crazy result of \p task + one
     */
    inline int fulfillTask(int &task) override {
        // sleep a second, since adding one is quite hard
        QThread::sleep(1);
        return task + 1;
    }

    /**
     * Clones a given worker
     *
     * @return The cloned worker
     */
    inline std::unique_ptr<Worker<int, int>> clone() override {
        // does nothing special, and only uses the given default constructor,
        // since we are not implementing any more private members etc.
        return std::make_unique<ConcreteWorker>();
    }
};

#endif
