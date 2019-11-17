#include <QCoreApplication>
#include <QMutex>
#include <QWaitCondition>
#include "../../Controller.h"
#include "ConcreteWorker.h"
#include "ConcreteProcessor.h"
#include "Communicator.h"


int main(int argc, char *argv[]) {
    // setup Qt stuff
    QCoreApplication a(argc, argv);
    // QMutex with QMutexLocker needed for our QWaitCondition - see: https://doc.qt.io/qt-5/qwaitcondition.html
    QMutex qMutex;
    QMutexLocker qMutexLocker(&qMutex);
    QWaitCondition qWaitCondition;

    // create Processor, Worker and Signals of our own Signals & Slots mechanism
    // in order to be able to communicate with the Processor
    std::unique_ptr<ConcreteProcessor> processor = std::make_unique<ConcreteProcessor>(qWaitCondition);
    std::unique_ptr<ConcreteWorker> worker = std::make_unique<ConcreteWorker>();

    Signal<const std::deque<int> &> signal_tasks;
    signal_tasks.registerSlot(
            processor.get(), &ConcreteProcessor::giveNewTasks, Qt::BlockingQueuedConnection
    );

    Signal<const std::string &> signal_message;
    signal_message.registerSlot(
            processor.get(), &ConcreteProcessor::setNewMessageToPrint, Qt::BlockingQueuedConnection
    );

    // create Communicator used for emitting signals to the Processor
    Communicator communicator;

    // setup Qt Signal - Slot connections between Communicator and Processor
    QObject::connect(
            &communicator, &Communicator::askForNewMessageToPrint,
            processor.get(), &ConcreteProcessor::setNewMessageToPrint,
            Qt::BlockingQueuedConnection
    );
    QObject::connect(
            &communicator, &Communicator::askToSetNewTasks,
            processor.get(), &ConcreteProcessor::giveNewTasks,
            Qt::BlockingQueuedConnection
    );
    QObject::connect(
            &communicator, &Communicator::askToClearTasks,
            processor.get(), &ConcreteProcessor::clearTasks,
            Qt::BlockingQueuedConnection
    );
    QObject::connect(
            &communicator, &Communicator::askToSetThreads,
            processor.get(), &ConcreteProcessor::setThreads,
            Qt::BlockingQueuedConnection
    );

    // See: https://doc.qt.io/qt-5/qthread.html#idealThreadCount
    const std::size_t numberOfThreadsToUse = QThread::idealThreadCount();
    // setup Controller with the already created Processor and Worker
    // notice, that we have to std::move (https://en.cppreference.com/w/cpp/utility/move)
    // the unique_ptr of the Processor and Worker
    Controller<int, int> controller(std::move(processor), std::move(worker), numberOfThreadsToUse);

    // ~~~~~ start doing cool stuff with the framework ~~~~~

    // fulfilling the tasks should take about 10 seconds, since each task takes about 1 second
    std::size_t numberOfTasksToGive = numberOfThreadsToUse * 10;
    // create a std::deque containing the tasks
    std::deque<int> tasks;
    for (std::size_t i = 0; i < numberOfTasksToGive; ++i) {
        tasks.emplace_back(i);
    }
    // start working on the tasks, using the Qt Signal - Slot mechanism
    emit communicator.askToSetNewTasks(tasks);

    // wait 5 seconds, so that about 50% of the work should be done
    QThread::sleep(5);

    // for the rest of the 50% of the work, a new message should be prepended
    std::string messageToPrint = "cool new message";
    // using the Qt Signal - Slot mechanism
    emit communicator.askForNewMessageToPrint(messageToPrint);

    // wait until everything is done
    qWaitCondition.wait(&qMutex);

    // set threads to use to 0 using the Qt Signal - Slot mechanism
    emit communicator.askToSetThreads(0);

    // start same tasks but using our own Signal & Slot mechanism instead of the Qt Signal & Slot mechanism
    // equivalent to "emit communicator.askToSetNewTasks(tasks);"
    signal_tasks(tasks);

    // notify user, that nothing new is being printed, since we are not using any threads now
    std::cout << "using 0 threads for 5 seconds" << std::endl;

    // wait 5 seconds, nothing should be done, since we are not using any threads
    QThread::sleep(5);

    // start using 1 thread and set a new message using the Qt Signal - Slot mechanism
    messageToPrint = "using 1 thread";
    emit communicator.askForNewMessageToPrint(messageToPrint);
    emit communicator.askToSetThreads(1);

    // wait 5 seconds, not much should be done, since we are using only 1 thread
    QThread::sleep(5);

    // finally use more threads again :) (unless you are really using a single core cpu)
    // using the Qt Signal - Slot mechanism
    emit communicator.askToSetThreads(numberOfThreadsToUse);

    // set a new message using our own Signal - Slot mechanism
    // equivalent to "emit communicator.askForNewMessageToPrint(messageToPrint);"
    messageToPrint = "using " + std::to_string(numberOfThreadsToUse) + " thread(s)";
    signal_message(messageToPrint);

    // wait until everything is done
    qWaitCondition.wait(&qMutex);

    return 0;
}
