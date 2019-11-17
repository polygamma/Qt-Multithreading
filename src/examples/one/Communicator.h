#ifndef QT_MULTITHREADING_COMMUNICATOR_H
#define QT_MULTITHREADING_COMMUNICATOR_H

#include <QObject>
#include <deque>

/**
 * Class used to "simulate" a more complex program. \n
 * We are going to emit signals to give orders to the ConcreteProcessor via this class,
 * really just to show, that it is possible, not because it is useful.
 */
class Communicator : public QObject {
Q_OBJECT
public:
    Communicator() : QObject(nullptr) {}

signals:

    /**
     * Going to set a new message to prepend when receiving results
     */
    void askForNewMessageToPrint(const std::string &);

    /**
     * Going to queue new tasks
     */
    void askToSetNewTasks(const std::deque<int> &);

    /**
     * Going to delete all currently queued tasks
     */
    void askToClearTasks();

    /**
     * Going to set a new number of threads/workers to use
     */
    void askToSetThreads(std::size_t);
};

#endif
