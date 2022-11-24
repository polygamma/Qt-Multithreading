#ifndef QT_MULTITHREADING_MAGIC_H
#define QT_MULTITHREADING_MAGIC_H

#include <QObject>
#include <QThread>
#include <QMetaObject>
#include <QRecursiveMutex>
#include <QMutex>
#include <QMutexLocker>
#include <utility>
#include <functional>
#include <unordered_map>
#include <vector>
#include <deque>
#include <tuple>
#include <unordered_set>

/**
 * This function allows to invoke another function in the event loop of a given \p context
 * without using Qt Signals or Slots.
 *
 * @tparam Callable The function to invoke
 * @tparam Args The arguments to invoke the function \p Callable with
 * @param context The context whose event loop is being used for execution
 * @param connectionType The \c Qt::ConnectionType to use - see: https://doc.qt.io/qt-5/qt.html#ConnectionType-enum
 * @param callable See \p Callable
 * @param args See \p Args
 * @return \p true if \p Callable has been invoked, \p false otherwise
 */
template<typename Callable, typename ...Args>
inline bool invokeInContext(QObject *const context, const Qt::ConnectionType connectionType, const Callable callable,
                            Args &&... args) {
    /**
     * Notice, that this function makes use of std::forward() (https://en.cppreference.com/w/cpp/utility/forward)
     * in combination with "forwarding references" (https://en.cppreference.com/w/cpp/language/reference#Forwarding_references)
     *
     * It is also being distinguished whether the invoked function call is happening in the same thread or
     * a Qt::BlockingQueuedConnection is used (if case)
     * or in another thread without a Qt::BlockingQueuedConnection (else case).
     *
     * This leads to using different lambdas (https://en.cppreference.com/w/cpp/language/lambda)
     * allowing the compiler not to make unneeded constructor calls for the arguments of the function to invoke.
     *
     * All in all allows this function to use the communication system provided by the Qt Framework "Signals & Slots"
     * (https://doc.qt.io/qt-5/signalsandslots.html) without having to use the just mentioned "Signals & Slots".
     *
     * We are thus able to eliminate 2/3 of the things on which the
     * Qt Meta-Object System (https://doc.qt.io/qt-5/metaobjects.html) is based on. \n
     * We do not need the Q_OBJECT macro, since we are not using Signals or Slots, and thus does the Meta-Object compiler
     * not generate any meta-object code.
     *
     * With only using the QObject class, we are able to use C++ templates in combination with the communication system
     * provided by the Qt Framework, which is normally not possible (https://doc.qt.io/archives/qt-4.8/templates.html).
     *
     * We are going to recreate the Signals and Slots mechanism of the Qt framework, so that we are able to use it
     * without problems in C++ templates and do not even have to register the data types we are going to send to other threads
     * with the meta-object system, which means: \n
     * No Q_DECLARE_METATYPE() (https://doc.qt.io/qt-5/qmetatype.html#Q_DECLARE_METATYPE)
     * and no qRegisterMetaType() (https://doc.qt.io/qt-5/qmetatype.html#qRegisterMetaType-1)
     *
     * The calls to the copy- and move constructors needed to transfer our data to another thread are being handled
     * by the C++ compiler when creating the lambda functions and no longer by the Qt moc compiler,
     * that's why the mentioned macro and function call can be completely avoided.
     *
     * The ONLY drawback is, that we have to keep track of the pointers to the functions (slots) we are going to invoke
     * and the pointers to the objects (context) to which the mentioned functions belong,
     * which is normally done by the Qt framework when using
     * the QObject::connect() (https://doc.qt.io/qt-5/qobject.html#connect) function.
     * Emitting a signal is then equivalent to calling this function with the just mentioned parameters.
     *
     * Last notice: When using this function to invoke with a
     * "pointer-to-member-function" (https://isocpp.org/wiki/faq/pointers-to-members), one has to provide a pointer to
     * the object on which to call the member function as first argument of \p Args as being described
     * here: https://isocpp.org/wiki/faq/pointers-to-members#macro-for-ptr-to-memfn \n
     * or here https://en.cppreference.com/w/cpp/utility/functional/invoke
     * and here https://en.cppreference.com/w/cpp/language/pointer#Pointers_to_member_functions
     *
     * tl;dr C++ lambda, C++ template and QMetaObject magic to recreate the Qt communication system without having to
     * fire up the Qt Meta-Object compiler. This function may seem like almost nothing, but actually round about
     * 10 hours of work went into this.
     */
    if (connectionType == Qt::DirectConnection || connectionType == Qt::BlockingQueuedConnection ||
        (connectionType == Qt::AutoConnection && QThread::currentThread() == context->thread())) {

        return QMetaObject::invokeMethod(
                context, [&]() -> void { std::invoke(callable, std::forward<Args>(args)...); }, connectionType
        );
    } else {

        return QMetaObject::invokeMethod(
                context,
                [=, ...args = std::forward<Args>(args)]() mutable -> void {
                    std::invoke(callable, std::forward<Args>(args)...);
                },
                connectionType
        );
    }
}

/**
 * Template function to get the corresponding void* of a member function
 *
 * @tparam Return The type of the return value
 * @tparam Class The class to which the member belongs
 * @tparam Args The arguments of the member function
 * @param f The member function itself
 * @return The void* of \p f
 */
template<typename Return, typename Class, typename ...Args>
inline void *functionToPointer(Return (Class::*f)(Args...)) {
    union {
        Return (Class::*rf)(Args...);

        void *cf = nullptr;
    };
    rf = f;
    return cf;
}

/**
 * Template function to get the corresponding void* of a function or a static member function
 *
 * @tparam Return The type of the return value
 * @tparam Args The arguments of the function or the static member function
 * @param f The function or static member function itself
 * @return The void* of \p f
 */
template<typename Return, typename ...Args>
inline void *functionToPointer(Return (*f)(Args...)) {
    union {
        Return (*rf)(Args...);

        void *cf = nullptr;
    };
    rf = f;
    return cf;
}

class SlotProvider;

/**
 * Provides the interface for the template class Signal, which is the usual C++ pure virtual hack
 *
 * This interface is needed,
 * since we are writing the framework header only without ever separating declaration and definition.
 *
 * For the descriptions of the methods, refer to the template class Signal itself
 */
class SignalProvider {
public:
    virtual ~SignalProvider() = default;

private:
    virtual void disconnectLocalInSignal(void *slot, SlotProvider *slotProvider) = 0;

    virtual std::unordered_set<SlotProvider *> getConnectedSlots() = 0;

    friend class SlotProvider;
};

/**
 * Class providing the functionality, so that instances of this class may be given to registerSlot() as parameter \p context
 *
 * In other words: If you want to use the event loop of a given object \p obj to execute functions to be registered
 * with registerSlot() of the Signal class, that object \p obj has to inherit from SlotProvider
 */
class SlotProvider : public QObject {
public:
    /**
     * Constructor allowing to set a QObject as parent - see: https://doc.qt.io/qt-5/qobject.html#QObject
     *
     * @param parent The pointer to the QObject to be set as parent, if wanted
     */
    explicit SlotProvider(QObject *const parent = nullptr) : QObject(parent) {}

    /**
     * Destructor which notifies all SignalProvider, that are connected in any way to this object, about the destruction
     */
    ~SlotProvider() override {
        disconnect(nullptr, nullptr, this);
    }

    /**
     * If and only if being called from within a Slot triggered by a SignalProvider,
     * returns the pointer to that SignalProvider. \n
     * A nullptr is being returned otherwise.
     *
     * @return The pointer to the SignalProvider, that triggered the Slot, nullptr if called in other situations.
     */
    inline SignalProvider *signalSender() {
        QMutexLocker signalSendersLocker(&this->signalSendersMutex);
        QThread *const executingThread = this->thread();

        if (this->signalSenders.count(executingThread)) {
            auto &queue = this->signalSenders[executingThread];
            if (queue.empty()) {
                return nullptr;
            } else {
                return queue.back();
            }
        } else {
            return nullptr;
        }
    }

    /**
     * See disconnect(void* const, SignalProvider* const, SlotProvider* const)
     *
     * @tparam Ret
     * @tparam C
     * @tparam Args
     * @param slot
     * @param signalProvider
     * @param slotProvider
     */
    template<typename Ret, typename C, typename ...Args>
    static inline void
    disconnect(Ret (C::* const slot)(Args...), SignalProvider *const signalProvider, SlotProvider *const slotProvider) {
        disconnect(functionToPointer(slot), signalProvider, slotProvider);
    }

    /**
     * See disconnect(void* const, SignalProvider* const, SlotProvider* const)
     *
     * @tparam Ret
     * @tparam Args
     * @param slot
     * @param signalProvider
     * @param slotProvider
     */
    template<typename Ret, typename ...Args>
    static inline void
    disconnect(Ret (*const slot)(Args...), SignalProvider *const signalProvider, SlotProvider *const slotProvider) {
        disconnect(functionToPointer(slot), signalProvider, slotProvider);
    }

    /**
     * Removes connections. If any of the arguments \p slot, \p signalProvider, \p slotProvider is nullptr,
     * that counts as wildcard, thus matches any regarding entry. \n
     * If they are not nullptr, the entries have to match exactly.
     *
     * At least one of the two parameters \p signalProvider or \p slotProvider has to be not nullptr.
     *
     * @param slot The pointer to a Slot, whose connections should be removed
     * @param signalProvider The pointer to a SignalProvider, whose connections should be removed
     * @param slotProvider The pointer to a SlotProvider, whose connections should be removed
     */
    static inline void
    disconnect(void *const slot, SignalProvider *const signalProvider, SlotProvider *const slotProvider) {
        if (!signalProvider && !slotProvider) {
            return;
        }

        QMutexLocker globalLocker(&SlotProvider::globalMutex);

        if (signalProvider && slotProvider) {
            signalProvider->disconnectLocalInSignal(slot, slotProvider);
            slotProvider->disconnectLocalInSlotProvider(slot, signalProvider);
        } else if (slotProvider) {
            for (const auto &connectedSignal : slotProvider->getConnectedSignals()) {
                connectedSignal->disconnectLocalInSignal(slot, slotProvider);
            }
            slotProvider->disconnectLocalInSlotProvider(slot, nullptr);
        } else {
            for (const auto &connectedSlot : signalProvider->getConnectedSlots()) {
                connectedSlot->disconnectLocalInSlotProvider(slot, signalProvider);
            }
            signalProvider->disconnectLocalInSignal(slot, nullptr);
        }
    }

private:
    /**
     * A global mutex, that has to be locked EVERY TIME
     * a SlotProvider accesses a SignalProvider or vice versa to modify the internal state of the other object. \n
     * E.g. on destruction of an object to deregister connections or when registering new connections. \n
     *
     * If locking of other mutexes is needed, too, this mutex HAS TO BE LOCKED FIRST. ALWAYS!
     * We don't want any deadlocks.
     */
    static inline QMutex globalMutex = QMutex();
    /**
     * A map containing pointers to SignalProvider that triggered Slots - stored according to the LIFO principle. \n
     * Assigned to the calling threads.
     */
    std::unordered_map<QThread *, std::deque<SignalProvider *>> signalSenders;
    /**
     * A map containing the Slots, to which SignalProvider have been registered.
     * Obviously mapped to those SignalProvider.
     */
    std::unordered_map<void *, std::vector<SignalProvider *>> slotsToConnectedSignals;
    /**
     * A mutex that has to be locked, when working with the SlotProvider::signalSenders
     */
    QMutex signalSendersMutex = QMutex();
    /**
     * A mutex that has to be locked, when working with the SlotProvider::slotsToConnectedSignals
     */
    QMutex slotsToConnectedSignalsMutex = QMutex();

    /**
     * Removes connections to this object. \n
     * If \p slot or \p signalProvider is nullptr, that counts as wildcard, thus matches any regarding entry. \n
     * If \p slot or \p signalProvider is not nullptr, the entries have to match exactly.
     *
     * @param slot The pointer to a Slot, whose connections should be removed
     * @param signalProvider The pointer to a SignalProvider, whose connections should be removed
     */
    inline void disconnectLocalInSlotProvider(void *const slot, SignalProvider *const signalProvider) {
        QMutexLocker slotsToConnectedSignalsLocker(&this->slotsToConnectedSignalsMutex);

        if (!slot && !signalProvider) {
            this->slotsToConnectedSignals.clear();
        } else if (slot && !signalProvider) {
            this->slotsToConnectedSignals.erase(slot);
        } else if (!slot) {
            for (auto &key_value : this->slotsToConnectedSignals) {
                for (auto it = key_value.second.begin(); it != key_value.second.end(); ++it) {
                    if (*it == signalProvider) {
                        key_value.second.erase(it);
                        break;
                    }
                }
            }
        } else if (this->slotsToConnectedSignals.count(slot)) {
            auto &signalProviders = this->slotsToConnectedSignals[slot];
            for (auto it = signalProviders.begin(); it != signalProviders.end(); ++it) {
                if (*it == signalProvider) {
                    signalProviders.erase(it);
                    return;
                }
            }
        }
    }

    /**
     * Returns all SignalProvider, that are connected to this SlotProvider in any way.
     *
     * @return A set containing the pointers to the SignalProvider connected to this SlotProvider in any way
     */
    inline std::unordered_set<SignalProvider *> getConnectedSignals() {
        QMutexLocker slotsToConnectedSignalsLocker(&this->slotsToConnectedSignalsMutex);
        std::unordered_set<SignalProvider *> connectedSignals;

        for (const auto &key_value : this->slotsToConnectedSignals) {
            for (const auto &connectedSignal : key_value.second) {
                connectedSignals.insert(connectedSignal);
            }
        }

        return connectedSignals;
    }

    /**
     * Registers a new connection. \n
     * It is assumed, that the connection does not already exist.
     *
     * @param slot The pointer to the connected Slot
     * @param signalProvider The pointer to the connected SignalProvider
     */
    inline void registerConnection(void *const slot, SignalProvider *const signalProvider) {
        QMutexLocker slotsToConnectedSignalsLocker(&this->slotsToConnectedSignalsMutex);

        if (this->slotsToConnectedSignals.count(slot)) {
            this->slotsToConnectedSignals[slot].emplace_back(signalProvider);
        } else {
            this->slotsToConnectedSignals[slot] = std::vector{signalProvider};
        }
    }

    /**
     * Is being called by a SignalProvider to execute a Slot. \n
     * Tracks the emitting SignalProvider, so that invoking signalSender() returns a pointer to that SignalProvider,
     * when being called while within that Slot.
     *
     * @tparam Callable The slot to invoke
     * @tparam Args The arguments for the \p Callable to be executed with
     * @param signalSender A pointer to the emitting SignalProvider
     * @param callable See \p Callable
     * @param args See \p Args
     */
    template<typename Callable, typename ...Args>
    inline void callSlot(SignalProvider *const signalSender, const Callable callable, Args &&... args) {
        QThread *const executingThread = this->thread();
        {
            QMutexLocker signalSendersLocker(&this->signalSendersMutex);

            if (this->signalSenders.count(executingThread)) {
                this->signalSenders[executingThread].emplace_back(signalSender);
            } else {
                this->signalSenders[executingThread] = std::deque{signalSender};
            }
        }
        std::invoke(callable, std::forward<Args>(args)...);
        {
            QMutexLocker signalSendersLocker(&this->signalSendersMutex);

            this->signalSenders[executingThread].pop_back();
        }
    }

    template<typename ...> friend
    class Signal;
};

/**
 * A template class used to emit Signals and thus call connected Slots.
 *
 * @tparam Args The arguments of the Signal and thus the arguments of the Slots connected to this Signal,
 *              since they have to match.
 */
template<typename ...Args>
class Signal final : public SignalProvider, public SlotProvider {
public:
    /**
     * Constructor allowing to set a QObject as parent - see: https://doc.qt.io/qt-5/qobject.html#QObject
     *
     * @param parent The pointer to the QObject to be set as parent, if wanted
     */
    explicit Signal<Args...>(QObject *const parent = nullptr) : SlotProvider(parent) {}

    /**
     * Constructor allowing to create a Signal based on the arguments of a given function.
     *
     * @tparam Ret Only needed for template parameter deduction, do not worry about it
     * @tparam C Only needed for template parameter deduction, do not worry about it
     * @param parent The pointer to the QObject to be set as parent, if wanted
     */
    template<typename Ret, typename C>
    explicit Signal<Args...>(Ret (C::* const )(Args...), QObject *const parent = nullptr) : Signal<Args...>(parent) {}

    /**
     * Constructor allowing to create a Signal based on the arguments of a given function.
     *
     * @tparam Ret Only needed for template parameter deduction, do not worry about it
     * @param parent The pointer to the QObject to be set as parent, if wanted
     */
    template<typename Ret>
    explicit Signal<Args...>(Ret (*const )(Args...), QObject *const parent = nullptr) : Signal<Args...>(parent) {}

    /**
     * Destructor which notifies all SlotProvider, that are connected in any way to this object, about the destruction
     */
    ~Signal() final {
        disconnect(nullptr, static_cast<SignalProvider *>(this), nullptr);
    }

    /**
     * Overloaded () operator, which emits the Signal and thus calls the connected Slots.
     *
     * @tparam SlotArgs The arguments to emit with the Signal
     * @param args See \p SlotArgs
     */
    template<typename ...SlotArgs>
    inline void operator()(SlotArgs &&... args) {
        QMutexLocker connectedSlotsLocker(&this->connectedSlotsMutex);

        for (const auto &key_value : this->connectedSlots) {
            for (const auto &slotToInvokeTuple : key_value.second) {
                std::get<1>(slotToInvokeTuple)(std::forward<SlotArgs>(args)...);
            }
        }
    }

    /**
     * Allows to use this Signal as Slot (also as Qt Slot). \n
     * We are thus able to connect Signal to Signal and to forward Qt Signals to our own Signal.
     *
     * Example: \n
     * Signal<> signal1, signal2; \n
     * signal1.registerSlot(&signal2, signal2.toSlot(), Qt::AutoConnection); // emitting signal1 emits signal2
     *
     * @return The Slot representation of this Signal
     */
    inline auto toSlot() {
        return &Signal::operator() < Args... >;
    }

    /**
     * Registers a new Slot with this Signal.
     *
     * @tparam Ret Only needed for template parameter deduction, do not worry about it
     * @tparam C The type of the SlotProvider, has to match the type of the \p context pointer
     * @tparam SlotArgs The arguments of the Slot to be connected
     * @param context   The SlotProvider whose context is going to be used for execution of the Slot,
     *                  which is not relevant in the case of Qt::DirectConnection
     * @param f The Slot to connect to
     * @param connectionType See: https://doc.qt.io/qt-5/qt.html#ConnectionType-enum
     */
    template<typename Ret, typename C, typename ...SlotArgs>
    inline void
    registerSlot(C *const context, Ret (C::* const f)(SlotArgs...), const Qt::ConnectionType connectionType) {
        QMutexLocker globalLocker(&SlotProvider::globalMutex);
        void *slot = functionToPointer(f);
        auto *slotProvider = static_cast<SlotProvider *>(context);

        if (connectionExists(slotProvider, slot)) {
            return;
        }

        appendConnection(slotProvider, slot, std::function([=, this](Args &&... args) mutable -> void {
            invokeInContext(
                    context, connectionType,
                    &SlotProvider::callSlot<Ret(C::*)(SlotArgs...), C *, Args...>, context, this,
                    f, static_cast<C *const>(context), std::forward<Args>(args)...
            );
        }));
    }

    /**
     * Registers a new Slot with this Signal.
     *
     * @tparam Ret Only needed for template parameter deduction, do not worry about it
     * @tparam C The type of the SlotProvider
     * @tparam SlotArgs The arguments of the Slot to be connected
     * @param context   The SlotProvider whose context is going to be used for execution of the Slot,
     *                  which is not relevant in the case of Qt::DirectConnection
     * @param f The Slot to connect to
     * @param connectionType See: https://doc.qt.io/qt-5/qt.html#ConnectionType-enum
     */
    template<typename Ret, typename C, typename ...SlotArgs>
    inline void
    registerSlot(C *const context, Ret (*const f)(SlotArgs...), const Qt::ConnectionType connectionType) {
        QMutexLocker globalLocker(&SlotProvider::globalMutex);
        void *slot = functionToPointer(f);
        auto *slotProvider = static_cast<SlotProvider *>(context);

        if (connectionExists(slotProvider, slot)) {
            return;
        }

        appendConnection(slotProvider, slot, std::function([=, this](Args &&... args) mutable -> void {
            invokeInContext(
                    context, connectionType,
                    &SlotProvider::callSlot<Ret(*)(SlotArgs...), Args...>, context, this,
                    f, std::forward<Args>(args)...
            );
        }));
    }

private:
    /**
     * A map containing the Slots together with the SlotProvider connected to this Signal
     */
    std::unordered_map<SlotProvider *, std::vector<std::tuple<void *, std::function<void(Args...)>>>> connectedSlots;
    /**
     * A mutex that has to be locked, when working with the Signal::connectedSlots
     */
    QRecursiveMutex connectedSlotsMutex = QRecursiveMutex();

    /**
     * Removes connections to this object. \n
     * If \p slot or \p slotProvider is nullptr, that counts as wildcard, thus matches any regarding entry. \n
     * If \p slot or \p slotProvider is not nullptr, the entries have to match exactly.
     *
     * @param slot The pointer to a Slot, whose connections should be removed
     * @param slotProvider The pointer to a SlotProvider, whose connections should be removed
     */
    inline void disconnectLocalInSignal(void *const slot, SlotProvider *const slotProvider) final {
        QMutexLocker connectedSlotsLocker(&this->connectedSlotsMutex);

        if (!slot && !slotProvider) {
            this->connectedSlots.clear();
        } else if (!slot) {
            this->connectedSlots.erase(slotProvider);
        } else if (!slotProvider) {
            for (auto &key_value : this->connectedSlots) {
                for (auto it = key_value.second.begin(); it != key_value.second.end(); ++it) {
                    if (std::get<0>(*it) == slot) {
                        key_value.second.erase(it);
                        break;
                    }
                }
            }
        } else if (this->connectedSlots.count(slotProvider)) {
            auto &toInvokeTuples = this->connectedSlots[slotProvider];
            for (auto it = toInvokeTuples.begin(); it != toInvokeTuples.end(); ++it) {
                if (std::get<0>(*it) == slot) {
                    toInvokeTuples.erase(it);
                    return;
                }
            }
        }
    }

    /**
     * Returns all SlotProvider, that are connected to this Signal in any way.
     *
     * @return A set containing the pointers to the SlotProvider connected to this Signal in any way
     */
    inline std::unordered_set<SlotProvider *> getConnectedSlots() final {
        QMutexLocker connectedSlotsLocker(&this->connectedSlotsMutex);
        std::unordered_set<SlotProvider *> connectedSlotsToReturn;

        for (const auto &key_value : this->connectedSlots) {
            connectedSlotsToReturn.insert(key_value.first);
        }

        return connectedSlotsToReturn;
    }

    /**
     * Returns, whether the specified connection already exists
     *
     * @param slotProvider The pointer to the potential SlotProvider
     * @param slot The pointer to the potential Slot
     * @return \p true if the connection already exists, \p false otherwise
     */
    inline bool connectionExists(SlotProvider *const slotProvider, void *const slot) {
        QMutexLocker connectedSlotsLocker(&this->connectedSlotsMutex);

        if (this->connectedSlots.count(slotProvider)) {
            const auto &toInvokeTuples = this->connectedSlots[slotProvider];
            for (const auto &toInvokeTuple : toInvokeTuples) {
                if (slot == std::get<0>(toInvokeTuple)) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Appends a new connection to this Signal and notifies the regarding SlotProvider about it
     *
     * @param slotProvider A pointer to the SlotProvider to be notified
     * @param slot A pointer to the Slot of the SlotProvider to be used
     * @param functionToAppend The callable, which is being used when emitting this Signal
     */
    inline void appendConnection(SlotProvider *const slotProvider, void *const slot,
                                 const std::function<void(Args...)> &functionToAppend) {
        QMutexLocker connectedSlotsLocker(&this->connectedSlotsMutex);

        if (this->connectedSlots.count(slotProvider)) {
            this->connectedSlots[slotProvider].emplace_back(slot, functionToAppend);
        } else {
            this->connectedSlots[slotProvider] = std::vector{std::make_tuple(slot, functionToAppend)};
        }

        slotProvider->registerConnection(slot, static_cast<SignalProvider *>(this));
    }
};

#endif
