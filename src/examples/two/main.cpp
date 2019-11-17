#include <iostream>
#include "../../Magic.h"

/**
 * Class to be used with both our own and the Signals & Slots system of Qt
 *
 * Inheriting from SlotProvider is the only thing needed in order to be able to connect instances of the class
 * to Signals of this framework - pretty easy, huh?
 *
 * Don't worry, SlotProvider itself is a subclass of QObject, so it's not excluding Qt stuff
 * as can be seen in the following example
 */
class SignalSlotProvider : public SlotProvider {
Q_OBJECT
public slots:

    /**
     * Boring function which can only print a std::string, since Qt cannot handle templates :(
     *
     * @param toPrint The std::string to print
     */
    void boringEcho(const std::string &toPrint) {
        std::cout << toPrint << std::endl;
    }

public:
    /**
     * Much cooler function which can print almost everything, it just needs to have an overloaded << operator
     * to be used with std::ostream
     *
     * @tparam T The type of the thing to print
     * @param toPrint The thing to print
     */
    template<typename T>
    void coolEcho(const T &toPrint) {
        std::cout << toPrint << " - i am a template function btw." << std::endl;
    }

signals:

    /**
     * Signal used to emit a std::string
     */
    void emitString(const std::string &);

};

int main() {
    SignalSlotProvider signalSlotProvider;

    // Let's start by connecting the Qt Signal and Slot.
    QObject::connect(
            &signalSlotProvider, &SignalSlotProvider::emitString, &signalSlotProvider, &SignalSlotProvider::boringEcho,
            Qt::AutoConnection
    );
    emit signalSlotProvider.emitString("this is not impressive");

    // --- Create a Signal of our framework, to emit the Qt Signal with it ---
    // make use of a constructor to create the template parameters
    // of our Signal based on the argument list of the Qt Signal
    Signal signalToEmitQtSignal(&SignalSlotProvider::emitString);
    // alternative to be explicit: Signal<const std::string &> signalToEmitQtSignal;

    // connect both signals
    signalToEmitQtSignal.registerSlot(&signalSlotProvider, &SignalSlotProvider::emitString, Qt::AutoConnection);
    // emit our signal using the overloaded () operator
    signalToEmitQtSignal("still not impressive");

    // --- Create another Signal of our framework, and use it as Slot for the Qt Signal ---
    Signal schroedingersSignal(&SignalSlotProvider::emitString);
    // alternative to be explicit: Signal<const std::string &> schroedingersSignal;
    // another alternative: Signal schroedingersSignal(signalToEmitQtSignal.toSlot());

    // use the .toSlot() method to simply use the Signal as Slot in a connection
    QObject::connect(
            &signalSlotProvider, &SignalSlotProvider::emitString, &schroedingersSignal, schroedingersSignal.toSlot(),
            Qt::AutoConnection
    );
    // to prove, that they are really meaningful connected, also connect schroedingersSignal to our template function
    schroedingersSignal.registerSlot(
            &signalSlotProvider, &SignalSlotProvider::coolEcho<std::string>, Qt::AutoConnection
    );
    // schroedingersSignal is now a Slot for the Qt Signal and a normal Signal connected to the template function
    signalToEmitQtSignal("i should be printed twice, by the boring function and the template function");

    // disconnect schroedingersSignal from the Qt Signal
    QObject::disconnect(&signalSlotProvider, nullptr, &schroedingersSignal, nullptr);
    signalToEmitQtSignal("i should be printed once, not by the template function");

    // connect schroedingersSignal to signalToEmitQtSignal used as Slot
    schroedingersSignal.registerSlot(&signalToEmitQtSignal, signalToEmitQtSignal.toSlot(), Qt::AutoConnection);
    // schroedingersSignal is now connected to the template function and to signalToEmitQtSignal
    // which emits the Qt Signal which is connected to the boring print function
    schroedingersSignal("i should be printed twice again, also by the template function");

    // as one can see, our Signals can be connected to Qt Signals & Slots without problems in both directions,
    // and obviously can Signals of our framework be used with other Signals of our framework in both directions, too
    // c++ templates are also no restriction for our own Signals

    // let's just disconnect stuff at the end
    // disconnect from signalToEmitQtSignal
    SlotProvider::disconnect(signalToEmitQtSignal.toSlot(), &schroedingersSignal, nullptr);
    schroedingersSignal("i should be printed once, only by the template function");

    // disconnect schroedingersSignal from the rest
    SlotProvider::disconnect(nullptr, &schroedingersSignal, nullptr);
    schroedingersSignal("you should not see me :(");

    // we have not disconnected the Qt Signal from the boringEcho function
    emit signalSlotProvider.emitString(
            "but you should see me, not printed by the template function, "
            "since how would that even be possible, Qt cannot handle such magic"
    );

    return 0;
}

#include "main.moc"
