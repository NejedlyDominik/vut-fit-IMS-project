/**
 * IMS - projekt
 * 
 * Adam Grunwald (xgrunw00), Dominik Nejedly (xnejed09), 2020
 * 
 * Model provozu pizzerie s dovazkou
 */

#include <iostream>
#include <getopt.h>
#include "simlib.h"

using namespace std;

#define MIN 60      // minuta
#define HOUR 3600   // hodina

#define DEFAULT_NUMBER_OF_RUNS 10   // vychozi pocet behu
#define DEFAULT_MEAL_NUMBER 400     // vychozi priblizny pocet jidel na smenu
#define DEFAULT_EMPLOYEES 3         // vychozi pocet zamestnancu
#define DEFAULT_FURNACE_CAPACITY 8  // vychozi pocet mist v peci
#define DEFAULT_CARS 2              // vychozi pocet aut

#define CAR_CAPACITY 8  // maximalni pocet jidel v aute

bool success;               // Nastaveno na "True", pokud smena skonci uspesne.
int successfulRuns = 0;     // pocet uspesnych smen

int generatedMeals;     // Pocet jidel, ktere bylo nutne za smenu pripravit.
int deliveredMeals;     // Pocet jidel, ktere se podarilo dorucit do konce smeny.

// cas pripravy jidel pro osobni odber - 1 smena
Stat takeawayMealStat("Takeaway meals - prepare time (s)");

// cas pripravy jidel od jejich prijeti po vyzvednuti ridicem - 1 smena
Stat mealForDeliveryStat("Meals for delivery - prepare time (S)");

// cas pripravy vsech jidel od pejich prijeti az po vyzvednuti ridicem, nebo osobni odber - 1 smena
Stat allMealsStat("All meals - prepare time (s)");

// souhrn vsech vyse zminenych statistik za vsechny simulovane behy
Stat takeawayMealStatSum("Takeaway meals summary - prepare time (s)");
Stat mealForDeliveryStatSum("Meals for delivery summary - prepare time (S)");
Stat allMealsStatSum("All meals summary - prepare time (s)");

// jidla cekajici na zabaleni
Queue readyMealsForPacking("Ready meals for packing");

// jidla cekajici na odber ridicem
Queue readyMealsForDelivery("Ready meals for delivery");


/**
 * auto rozvazejici hotova jidla urcena k doruceni
 */
class Car : public Process
{
private:
    int meals;
    Store *drivers;

public:
    Car(Store *drivers) : drivers(drivers){};

    void Behavior()
    {
        while (1)
        {
            // Ceka, dokud nejsou zadna jidla k rozvozu.
            WaitUntil(!readyMealsForDelivery.Empty());

            Enter(*drivers, 1);
            meals = 0;

            // rezie ridice pri prijezdu a nakladka
            Wait(MIN * Uniform(1, 3));

            // Nalozi svoji maximalni kapacitu nebo vsechna jidla cekajici na odvoz.
            while ((meals < CAR_CAPACITY) && (!readyMealsForDelivery.Empty()))
            {
                meals++;
                readyMealsForDelivery.GetFirst()->Activate();
            }

            // Pokud bylo vse odvezeno jinym autem, neodjizdi, ale znovu ceka, dokud nejsou pripravena jidla k rozvozu.
            if (meals > 0)
            {
                Wait(MIN * Normal(20, 3));
                deliveredMeals += meals;
            }

            Leave(*drivers, 1);
        }
    }
};


/**
 * zabaleni hotoveho jidla
 */
class Packing : public Process
{
private:
    Store *freeEmployees;

public:
    Packing(Store *freeEmployees) : Process(1), freeEmployees(freeEmployees){};

    void Behavior()
    {
        while (1)
        {
            // Ceka, dokud nejsou zadna jidla hotova.
            WaitUntil(!readyMealsForPacking.Empty());

            Enter(*freeEmployees, 1);

            // Zabali vsechna jidla hotova jidla.
            while (!readyMealsForPacking.Empty())
            {
                Wait(Uniform(10, 15));
                readyMealsForPacking.GetFirst()->Activate();
            }

            Leave(*freeEmployees, 1);
        }
    }
};


/**
 * proces pripraveni jidla k odberu
 */
class Meal : public Process
{
private:
    Store *freeEmployees;
    Store *freeFurnacePlaces;

public:
    Meal(Store *freeEmployees, Store *freeFurnacePlaces) : freeEmployees(freeEmployees), freeFurnacePlaces(freeFurnacePlaces){};

    void Behavior()
    {
        double creationTime = Time;

        // privava jidla zamestnancem
        Enter(*freeEmployees, 1);

        if (Uniform(0, 100) <= 40)
        {
            Wait(Normal(3.5 * MIN, 10));
        }
        else
        {
            Wait(Normal(2 * MIN, 5));
        }

        Leave(*freeEmployees, 1);

        // peceni jidla v peci
        Enter(*freeFurnacePlaces, 1);
        Wait(2 * MIN);
        Leave(*freeFurnacePlaces, 1);

        // zabaleni jidla
        readyMealsForPacking.Insert(this);
        Passivate();

        // Vyber, zdali je jidlo urceno k rozvozu, nebo osobnimu odberu.
        if (Uniform(0, 100) <= 65)
        {
            // doruceni jidla autem
            readyMealsForDelivery.Insert(this);
            Passivate();
            mealForDeliveryStat(Time - creationTime);
            mealForDeliveryStatSum(Time - creationTime);
        }
        else
        {
            takeawayMealStat(Time - creationTime);
            takeawayMealStatSum(Time - creationTime);
            deliveredMeals++;
        }

        allMealsStat(Time - creationTime);
        allMealsStatSum(Time - creationTime);
    }
};


/**
 * Proces konce smeny, kdy uz nejsou prijimany zadne objednavky.
 */
class shiftEnd : public Process
{
public:
    void Behavior()
    {
        // Ceka, dokud neni vse rozvezeno.
        WaitUntil(generatedMeals == deliveredMeals);

        success = true;
        successfulRuns++;
    }
};


/**
 * ukonceni generovani objednavek
 */
class StopOrderGenerator : public Event
{
private:
    Event *id;

public:
    StopOrderGenerator(Event *e, double dt) : id(e)
    {
        Activate(Time + dt);
    }

    void Behavior()
    {
        // Zrusi generator a aktivuje proces konce smeny.
        id->Cancel();
        (new shiftEnd)->Activate();
        Cancel();
    }
};


/**
 * generovani objednavek
 */
class OrderGenerator : public Event
{
private:
    double orderMeanTime;
    Store *freeEmployees;
    Store *freeFurnacePlaces;

public:
    OrderGenerator(double orderMeanTime, Store *freeEmployees, Store *freeFurnacePlaces, int acceptOrderTime) : orderMeanTime(orderMeanTime), freeEmployees(freeEmployees), freeFurnacePlaces(freeFurnacePlaces)
    {
        (new StopOrderGenerator(this, acceptOrderTime));
    }

    void Behavior()
    {
        // generovani jednotlivych jidel
        do
        {
            (new Meal(freeEmployees, freeFurnacePlaces))->Activate();
            generatedMeals++;
        } while (Uniform(0, 100) <= 50);

        Activate(Time + Exponential(orderMeanTime));
    }
};


/**
 * napoveda pri spatne zadanych argumentech
 */
void printAdvice()
{
    cerr << "\nUSAGE: ./ims_proj [-r number_of_runs] [-m meals_number] [-e employees] [-f furnace_capacity] [-c cars] [-s shift_time] [-a accept_orders_time]\n";
    cerr << "\nFor more detailed description use option \"-h\".\n";
}


/**
 * pomocna fuknce pro tisk minut
 */
void printMins(int time)
{
    int mins;

    if ((mins = (time % HOUR) / MIN) > 0)
    {
        cout << mins << " min ";
    }

    cout << "\n";
}


/**
 * kontrola a prevod vstupnich argumentu
 */
int checkArgs(char *arg)
{
    int num;
    char *endPtr;

    num = strtol(arg, &endPtr, 10);

    if (*endPtr != 0)
    {
        return -1;
    }

    return num;
}


int main(int argc, char **argv)
{
    int opt;
    double orderMeanTime;
    int numberOfRuns = DEFAULT_NUMBER_OF_RUNS;
    int mealsPerShift = DEFAULT_MEAL_NUMBER;
    int employees = DEFAULT_EMPLOYEES;
    int furnaceCapacity = DEFAULT_FURNACE_CAPACITY;
    int cars = DEFAULT_CARS;
    int shiftTime = 12 * HOUR;
    int acceptOrderTime = 11 * HOUR;
    int allGeneratedMeals = 0;
    int allDeliveredMeals = 0;
    double empWorkloadSum = 0;
    double furBakeloadSum = 0;
    double drivWorkloadSum = 0;

    // ziskani a kontrola vstupnich argumentu
    while ((opt = getopt(argc, argv, "r:m:e:f:c:s:a:h")) != -1)
    {
        switch (opt)
        {
        case 'h':
            cout << "DESCRIPTION: Pizzeria simulation focused on tracking the order from its placement to the delivery.\n";
            cout << "USAGE: ./ims_proj [-r number_of_runs] [-m meals_number] [-e employees] [-f furnace_capacity] [-c cars] [-s shift_time] [-a accept_orders_time]\n";
            cout << "OPTIONS:\n";
            cout << "\t-r\tnumber of runs\n";
            cout << "\t-m\tapproximate number of meals per shift\n";
            cout << "\t-e\tnumber of employees\n";
            cout << "\t-f\tfurnace capacity\n";
            cout << "\t-c\tnumber of cars\n";
            cout << "\t-s\tshift time (in minutes)\n";
            cout << "\t-a\tperiod of orders acceptance (in minutes)\n\n";
            cout << "The order of the parameters is arbitrary.\n";
            return EXIT_SUCCESS;

        case 'r':
            if ((numberOfRuns = checkArgs(optarg)) < 1)
            {
                cerr << "PARAM_ERROR: Invalid number of runs.\n\n";
                printAdvice();
                return EXIT_FAILURE;
            }

            break;

        case 'm':
            if ((mealsPerShift = checkArgs(optarg)) < 1)
            {
                cerr << "PARAM_ERROR: Invalid approximate number of meals per shift.\n\n";
                printAdvice();
                return EXIT_FAILURE;
            }

            break;

        case 'e':
            if ((employees = checkArgs(optarg)) < 1)
            {
                cerr << "PARAM_ERROR: Invalid number of employees.\n";
                printAdvice();
                return EXIT_FAILURE;
            }

            break;

        case 'f':
            if ((furnaceCapacity = checkArgs(optarg)) < 1)
            {
                cerr << "PARAM_ERROR: Invalid number of furnace capacity.\n";
                printAdvice();
                return EXIT_FAILURE;
            }

            break;

        case 'c':
            if ((cars = checkArgs(optarg)) < 1)
            {
                cerr << "PARAM_ERROR: Invalid number of cars.\n";
                printAdvice();
                return EXIT_FAILURE;
            }

            break;

        case 's':
            if ((shiftTime = (checkArgs(optarg) * MIN)) < 1)
            {
                cerr << "PARAM_ERROR: Invalid time of shift.\n";
                printAdvice();
                return EXIT_FAILURE;
            }

            break;

        case 'a':
            if ((acceptOrderTime = (checkArgs(optarg) * MIN)) < 1)
            {
                cerr << "PARAM_ERROR: Invalid period of orders acceptance.\n";
                printAdvice();
                return EXIT_FAILURE;
            }

            break;

        case '?':
            printAdvice();
            return EXIT_FAILURE;

        default:
            cerr << "PARAM_ERROR: Unknown option.\n";
            printAdvice();
            return EXIT_FAILURE;
        }
    }

    if (acceptOrderTime >= shiftTime)
    {
        cerr << "PARAM_ERROR: Period of accepting orders has to be shorter than shift time.\n";
        return EXIT_FAILURE;
    }

    if (optind < argc)
    {
        cerr << "PARAM_ERROR: Argument parsing failed.\n";
        return EXIT_FAILURE;
    }

    // vypocet stredniho casu pro generovani objednavek
    orderMeanTime = acceptOrderTime / (double(mealsPerShift) / 2);

    // volni zamestnanci
    Store freeEmployees("Employees store", employees);
    // volna mista v peci
    Store freeFurnacePlaces("Furnace store", furnaceCapacity);
    // volni ridici
    Store drivers("Drivers store", cars);

    cout << "\n============================================================\n";
    cout << "SIMULATION START\n";
    cout << "------------------------------------------------------------\n";
    cout << "approximate number of meals per shift:  " << mealsPerShift << "\n";
    cout << "mean time value of orders:              " << orderMeanTime << "\n";
    cout << "number of employees:                    " << employees << "\n";
    cout << "furnace capacity:                       " << furnaceCapacity << "\n";
    cout << "number of cars:                         " << cars << "\n";
    cout << "shift time:                             " << shiftTime / HOUR << " h ";
    printMins(shiftTime);
    cout << "period of order acceptance:             " << acceptOrderTime / HOUR << " h ";
    printMins(acceptOrderTime);
    cout << "============================================================\n";

    // jednotlive behy simulace (smeny)
    for (int i = 0; i < numberOfRuns; i++)
    {
        cout << "\n------------------------------------------------------------\n";
        cout << "RUN " << i + 1 << "\n";
        cout << "------------------------------------------------------------\n\n";

        Init(0, shiftTime);

        readyMealsForDelivery.Clear();
        freeEmployees.Clear();
        freeFurnacePlaces.Clear();
        drivers.Clear();

        readyMealsForPacking.Clear();
        readyMealsForDelivery.Clear();

        takeawayMealStat.Clear();
        mealForDeliveryStat.Clear();
        allMealsStat.Clear();

        success = false;
        generatedMeals = 0;
        deliveredMeals = 0;

        // vytvoreni procesu aut
        for (int j = 0; j < cars; j++)
        {
            (new Car(drivers))->Activate();
        }

        // vytvoreni procesu baleni
        (new Packing(freeEmployees))->Activate();

        // vytvoreni generatoru objednavek a zahajeni simulace
        (new OrderGenerator(orderMeanTime, freeEmployees, freeFurnacePlaces, acceptOrderTime))->Activate();
        Run();

        // sbirani hodnot pro shrnuti
        allGeneratedMeals += generatedMeals;
        allDeliveredMeals += deliveredMeals;

        empWorkloadSum += freeEmployees.tstat.MeanValue();
        furBakeloadSum += freeFurnacePlaces.tstat.MeanValue();
        drivWorkloadSum += drivers.tstat.MeanValue();

        // tisk statistik
        cout << "Everything delivered before shift end: " << ((success) ? "SUCCESS" : "FAIL") << "\n\n";

        cout << "Number of generated meals: " << generatedMeals << "\n";
        cout << "Number of delivered meals: " << deliveredMeals << "\n\n";

        cout << "Employees workload:  " << freeEmployees.tstat.MeanValue() << "\n";
        cout << "Furnace \"bakeload\":  " << freeFurnacePlaces.tstat.MeanValue() << "\n";
        cout << "Drivers workload:    " << drivers.tstat.MeanValue() << "\n\n";

        takeawayMealStat.Output();
        cout << "\n";
        mealForDeliveryStat.Output();
        cout << "\n";
        allMealsStat.Output();
    }

    // Pokud probehlo vice simulacnich behu, jsou vytisknuty i jejich souhrne statistiky.
    if (numberOfRuns > 1)
    {
        cout << "\n============================================================\n";
        cout << "SUMMARY\n";
        cout << "------------------------------------------------------------\n\n";

        cout << "Success rate (%): " << (double(successfulRuns) / numberOfRuns) * 100 << "\n\n";

        cout << "Number of runs:   " << numberOfRuns << "\n";
        cout << "Successful runs:  " << successfulRuns << "\n\n";

        cout << "Average number of meals generated per shift: " << double(allGeneratedMeals) / numberOfRuns << "\n";
        cout << "Average number of meals delivered per shift: " << double(allDeliveredMeals) / numberOfRuns << "\n\n";

        cout << "Average employees workload per shift:  " << empWorkloadSum / numberOfRuns << "\n";
        cout << "Average furnace \"bakeload\" per shift:  " << furBakeloadSum / numberOfRuns << "\n";
        cout << "Average drivers workload per shift:    " << drivWorkloadSum / numberOfRuns << "\n\n";

        takeawayMealStatSum.Output();
        cout << "\n";
        mealForDeliveryStatSum.Output();
        cout << "\n";
        allMealsStatSum.Output();
    }

    cout << "\n============================================================\n";
    cout << "SIMULATION END\n";
    cout << "============================================================\n\n";

    return EXIT_SUCCESS;
}