#ifndef QT_MULTITHREADING_CONTROLLER_H
#define QT_MULTITHREADING_CONTROLLER_H

#include <QObject>
#include <QThread>
#include <memory>
#include <cstddef>
#include "WorkerController.h"
#include "Worker.h"
#include "Processor.h"

/**
 * Class to be used by the user of the framework to initialize a Processor together with a Worker. \n
 * The constructor of this class takes the ownership of the Processor and Worker and moves them to their own threads. \n
 * When the constructor returns, everything is set up and ready to use.
 * The Processor and Worker are dependent on the Controller, which means when the lifetime of the Controller ends
 * the Processor and Worker are deleted, too.
 *
 *
 * @tparam T The data type of the task to fulfill
 * @tparam R The data type of the result calculated during fulfilling the task
 */
template<typename T, typename R>
class Controller : public QObject {
public:
    /**
     * Constructor used to initialize the Controller
     *
     * @param processor The Processor to use
     * @param worker The prototype Worker which will be cloned
     * @param numberOfThreads The number of threads to use, which is the same as the number of Worker to use
     * @param parent    Another QObject to use as parent for the controller, if wanted.
     *                  See: https://doc.qt.io/qt-5/qobject.html#QObject
     */
    Controller<T, R>(std::unique_ptr<Processor<T, R>> processor, std::unique_ptr<Worker<T, R>> worker,
                     const std::size_t numberOfThreads, QObject *const parent = nullptr)
            : QObject(parent) {
        // setup WorkerController and the corresponding thread
        std::unique_ptr<WorkerController<T, R>> workerController = std::unique_ptr<WorkerController<T, R>>(
                new WorkerController<T, R>(std::move(worker), std::move(processor), numberOfThreads)
        );
        workerController->moveToThread(&this->workerControllerThread);
        this->workerControllerThread.start();
        QObject::connect(
                &this->workerControllerThread, &QThread::finished, workerController.release(), &QObject::deleteLater
        );
    }

    /**
     * Stops the WorkerController and thus everything else
     */
    ~Controller<T, R>() override {
        workerControllerThread.quit();
        workerControllerThread.wait();
    }

private:
    /**
     * Thread containing the WorkerController
     */
    QThread workerControllerThread;
};

#endif
