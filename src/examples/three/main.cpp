#include <iostream>
#include <QCoreApplication>
#include <QWaitCondition>
#include <QMutex>
#include <QThread>
#include "../../Controller.h"
#include "CollatzWorker.h"
#include "CollatzProcessor.h"


int main(int argc, char *argv[]) {
    // setup Qt stuff
    QCoreApplication a(argc, argv);
    QWaitCondition qWaitCondition;
    QMutex qMutex;
    QMutexLocker qMutexLocker(&qMutex);

    // setup CollatzWorker and CollatzProcessor
    std::unique_ptr<CollatzProcessor> processor = std::make_unique<CollatzProcessor>(qWaitCondition);
    std::unique_ptr<CollatzWorker> worker = std::make_unique<CollatzWorker>();

    // also store a ptr to the processor for later usage
    CollatzProcessor *const processorPtr = processor.get();

    // create signal which is going to receive the results
    Signal signal(processor->resultSignal.toSlot());
    processor->resultSignal.registerSlot(&signal, signal.toSlot(), Qt::AutoConnection);

    // we are going to process the results in the thread of the signal, which is currently the Qt main thread
    // notice, that &CollatzProcessor::printResult refers to a static member function, which allows
    // execution in the context of objects, that are not CollatzProcessor
    signal.registerSlot(&signal, &CollatzProcessor::printResult, Qt::AutoConnection);

    // create 4 more signals in order to be able to give commands to the processor
    // but we are actually only going to use two of the signals in this example
    // however: feel free to modify the code and and make use of the other signals, too
    Signal setThreadsSignal(&CollatzProcessor::setNumberThreads);
    setThreadsSignal.registerSlot(
            processor.get(), &CollatzProcessor::setNumberThreads, Qt::BlockingQueuedConnection
    );

    Signal extendTasksSignal(&CollatzProcessor::extendTasks);
    extendTasksSignal.registerSlot(
            processor.get(), &CollatzProcessor::extendTasks, Qt::BlockingQueuedConnection
    );

    Signal clearTasksSignal(&CollatzProcessor::clearTasks);
    clearTasksSignal.registerSlot(
            processor.get(), &CollatzProcessor::clearTasks, Qt::BlockingQueuedConnection
    );

    Signal processHereSignal(&CollatzProcessor::toProcessHere);
    processHereSignal.registerSlot(
            processor.get(), &CollatzProcessor::toProcessHere, Qt::BlockingQueuedConnection
    );

    // create controller and give processor + worker to it
    Controller<unsigned int, return_tuple> controller(
            std::move(processor), std::move(worker), QThread::idealThreadCount()
    );

    // move the signal to the thread of the processor
    // doing this achieves the same thing as actually processing the results directly in the processor
    // without emitting the result via a signal first, but since this is an example, we just want to show what's possible
    signal.moveToThread(processorPtr->thread());

    // create tasks and give them to the processor
    std::deque<unsigned int> tasks;
    for (unsigned int i = 1; i <= 15; ++i) {
        tasks.emplace_back(i);
    }

    std::cout << std::endl << "--- Start working on new tasks ---" << std::endl;
    extendTasksSignal(tasks);

    // wait until everything is done
    qWaitCondition.wait(&qMutex, 1000);
    std::cout << "--- We returned from the wait condition --- " << std::endl << std::endl;

    // we could also use the Qt main thread to process the results
    // this is possible since we do not have to process the results in the processor directly,
    // but allow emitting them via a signal, which is what we are currently using, but while processing them
    // in the same thread in which the processor itself is running

    // start with destroying the current connection first
    SlotProvider::disconnect(&CollatzProcessor::printResult, &signal, &signal);

    // create a new slotProvider, which remains in the Qt main thread and use it to process the results
    SlotProvider slotProvider;
    signal.registerSlot(&slotProvider, &CollatzProcessor::printResult, Qt::AutoConnection);

    // create new tasks
    tasks.clear();
    for (unsigned int i = 16; i <= 30; ++i) {
        tasks.emplace_back(i);
    }

    std::cout << std::endl << "--- Start working on new tasks ---" << std::endl;
    extendTasksSignal(tasks);

    // wait until everything is done
    qWaitCondition.wait(&qMutex, 1000);
    std::cout << "--- We returned from the wait condition --- " << std::endl;

    // we have to explicitly process the events, since we are working in the main Qt thread
    std::cout << "--- Start processing the event queue of the main thread --- " << std::endl << std::endl;
    QCoreApplication::processEvents();

    // now let's use the processor itself to handle the results, without emitting anything via the signal
    // create new tasks
    tasks.clear();
    for (unsigned int i = 31; i <= 45; ++i) {
        tasks.emplace_back(i);
    }
    processHereSignal(true);

    // show that we are really not using our signal, so destroy the connections
    SlotProvider::disconnect(nullptr, &signal, nullptr);
    SlotProvider::disconnect(nullptr, nullptr, &signal);

    std::cout << std::endl << "--- Start working on new tasks ---" << std::endl;
    extendTasksSignal(tasks);

    // wait until everything is done
    qWaitCondition.wait(&qMutex, 1000);
    std::cout << "--- We returned from the wait condition --- " << std::endl << std::endl;

    return 0;
}
