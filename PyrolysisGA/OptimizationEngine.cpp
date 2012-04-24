#include "OptimizationEngine.hpp"

std::vector<ExpData>* OptimizationEngine::DataSet = (std::vector<ExpData>*)0;
double* OptimizationEngine::ModData = (double*)0;

const double OptimizationEngine::A_initial = 5.49e12;
const double OptimizationEngine::E_initial = 1.70e5;
const double OptimizationEngine::NS_initial = 3.56;
const double OptimizationEngine::yinf_initial = 0.231642;

const int OptimizationEngine::SEED = 1337;
const int OptimizationEngine::POP_SIZE = 100; // Size of population
const int OptimizationEngine::MAX_GEN = 1000; // Maximum number of generation before STOP
const unsigned int OptimizationEngine::PRINT_EVERY_SEC = 10; //print info to console every specified second


const double OptimizationEngine::HYPER_CUBE_RATE = 0.5;     // relative weight for hypercube Xover
const double OptimizationEngine::SEGMENT_RATE = 0.5;  // relative weight for segment Xover
const double OptimizationEngine::ALFA = 10.0;     //BLX coefficient
const double OptimizationEngine::EPSILON = 0.1;	// range for real uniform mutation
const double OptimizationEngine::SIGMA = 0.3;	    	// std dev. for normal mutation
const double OptimizationEngine::UNIFORM_MUT_RATE = 0.5;  // relative weight for uniform mutation
const double OptimizationEngine::DET_MUT_RATE = 0.5;      // relative weight for det-uniform mutation
const double OptimizationEngine::NORMAL_MUT_RATE = 0.5;   // relative weight for normal mutation
const double OptimizationEngine::P_CROSS = 0.8;	// Crossover probability
const double OptimizationEngine::P_MUT = 0.5;	// mutation probability

void OptimizationEngine::Run(std::vector<ExpData>* dataSet)
{
    DataSet = dataSet;
    ModData = new double[dataSet->size()];

    rng.reseed(SEED);
    eoEvalFuncPtr<Indi, double, const std::vector<double>& > plainEval(FitnessFce);
    eoEvalFuncCounter<Indi> eval(plainEval);

    eoPop<Indi> pop;

    InitPop(pop, plainEval);

    pop.sort();

    std::cout << "Initial Population:" << std::endl;
    std::cout << "----------------------------" << std::endl;
    std::cout << pop;

    // eoDetTournamentSelect<Indi> selectOne(30); //deterministic tournament selection
    // eoProportionalSelect<Indi> selectOne; //roulette wheel selection
    eoStochTournamentSelect<Indi>  selectOne(0.8); //Stochastic Tournament

    eoSelectPerc<Indi> select(selectOne, 2.0);
    /*It will select floor(rate*pop.size()) individuals and pushes them to
    the back of the destination population.*/
    //eoSelectPerc<Indi> select(selectOne); //rate == 1.0

    //eoGenerationalReplacement<Indi> replace; //all offspring replace all parents
    eoPlusReplacement<Indi> replace; //the best from offspring+parents become the next generation

    //eoSSGAStochTournamentReplacement<Indi> replace(0.8);
    /*
    in which parents to be killed are chosen by a (reverse) stochastic tournament.
    Additional parameter (in the constructor) is the tournament rate, a double.
    */

    //Transformation
    eoSegmentCrossover<Indi> xoverS(ALFA);
    // uniform choice in hypercube built by the parents
    eoHypercubeCrossover<Indi> xoverA(ALFA);
    // Combine them with relative weights
    eoPropCombinedQuadOp<Indi> xover(xoverS, HYPER_CUBE_RATE);
    xover.add(xoverA, HYPER_CUBE_RATE);

    // MUTATION
    // offspring(i) uniformly chosen in [parent(i)-epsilon, parent(i)+epsilon]
    eoUniformMutation<Indi>  mutationU(EPSILON);
    // k (=1) coordinates of parents are uniformly modified
    eoDetUniformMutation<Indi>  mutationD(EPSILON);
    // all coordinates of parents are normally modified (stDev SIGMA)
    double sigma = SIGMA;
    eoNormalMutation<Indi>  mutationN(sigma);
    // Combine them with relative weights
    eoPropCombinedMonOp<Indi> mutation(mutationU, UNIFORM_MUT_RATE);
    mutation.add(mutationD, DET_MUT_RATE);
    mutation.add(mutationN, NORMAL_MUT_RATE);

    eoSGATransform<Indi> transform(xover, P_CROSS, mutation, P_MUT);

    eoGenContinue<Indi> genCont(MAX_GEN);

    eoCombinedContinue<Indi> continuator(genCont);

    //statistics
    // but now you want to make many different things every generation
    // (e.g. statistics, plots, ...).
    // the class eoCheckPoint is dedicated to just that:
    // Declare a checkpoint (from a continuator: an eoCheckPoint
    // IS AN eoContinue and will be called in the loop of all algorithms)
    eoCheckPoint<Indi> checkpoint(continuator);

    // Create a counter parameter
    eoValueParam<unsigned> generationCounter(0, "Gen.");

    // Create an incrementor (sub-class of eoUpdater). Note that the
    // parameter's value is passed by reference,
    // so every time the incrementer is updated (every generation),
    // the data in generationCounter will change.
    eoIncrementor<unsigned> increment(generationCounter.value());
    // Add it to the checkpoint,
    // so the counter is updated (here, incremented) every generation
    checkpoint.add(increment);
    // now some statistics on the population:
    // Best fitness in population
    eoBestFitnessStat<Indi> bestStat;
    // Second moment stats: average and stdev
    eoSecondMomentStats<Indi> SecondStat;
    // Add them to the checkpoint to get them called at the appropriate time
    checkpoint.add(bestStat);
    checkpoint.add(SecondStat);
    // The Stdout monitor will print parameters to the screen ...
    eoStdoutMonitor monitor;

    // when called by the checkpoint (i.e. at every generation)
    // checkpoint.add(monitor);
    eoTimedMonitor timed(PRINT_EVERY_SEC);
    timed.add(monitor);
    checkpoint.add(timed);

    // the monitor will output a series of parameters: add them
    monitor.add(generationCounter);
    // monitor.add(eval); // because now eval is an eoEvalFuncCounter!
    monitor.add(bestStat);
    monitor.add(SecondStat);
    // A file monitor: will print parameters to ... a File, yes, you got it!
    eoFileMonitor fileMonitor("stats.xy", " ");

    // the checkpoint mechanism can handle multiple monitors
    checkpoint.add(fileMonitor);
    // the fileMonitor can monitor parameters, too, but you must tell it!
    fileMonitor.add(generationCounter);
    fileMonitor.add(bestStat);
    fileMonitor.add(SecondStat);

    //final settings
    eoEasyEA<Indi> gga(checkpoint, eval, select, transform, replace);

    std::cout << "Working..." << std::endl;
    gga(pop); //GO!

    // OUTPUT
    // Print (sorted) intial population
    pop.sort();
    std::cout << "Final Population:" << std::endl;
    std::cout << "----------------------------" << std::endl;
    std::cout << pop;
    std::cout << "----------------------------" << std::endl;

    std::cout << "The Best member:" << std::endl;

    double fitness = pop[0].fitness();
    double A = (pop[0])[0];
    double E = (pop[0])[1];
    double NS = (pop[0])[2];
    double yinf = (pop[0])[3];

    std::cout << "Fitness: " << fitness << std::endl;
    std::cout << "A: " << A << std::endl;
    std::cout << "E: " << E << std::endl;
    std::cout << "NS: " << NS << std::endl;
    std::cout << "yinf: " << yinf << std::endl;

    std::cout << "----------------------------" << std::endl;

    SaveResults(fitness, A, E, NS, yinf);

    std::cout << "Results saved into results.xy" << std::endl;
    std::cout << "Statistics saved into stats.xy" << std::endl;

    delete[] ModData;
}

double OptimizationEngine::FitnessFce(const std::vector<double>& pars)
{
    double A = pars[0];
    double E = pars[1];
    double NS = pars[2];
    double yinf = pars[3];

    double fitness = 0.0;
    double delta;

    Integrator::Runge23(DataSet, ModData, A, E, NS, yinf);

    for(unsigned int i = 1; i < DataSet->size(); i++)
    {
        delta = (*DataSet)[i].MassFrac() - ModData[i];
        fitness -= delta*delta;
    }

    if(!std::isfinite(fitness))
        return -1e300;

    return fitness;
}

void OptimizationEngine::InitPop(eoPop<Indi>& pop, eoEvalFuncPtr<Indi, double, const std::vector<double>& > eval)
{
    for(int i = 0; i < POP_SIZE; i++)
    {
        Indi v;

        v.push_back(A_initial + rng.normal(A_initial*rng.uniform(0., 2.)));
        v.push_back(E_initial + rng.normal(E_initial*rng.uniform(0., 2.)));
        v.push_back(NS_initial + rng.normal(NS_initial*rng.uniform(0., 2.)));
        v.push_back(yinf_initial + rng.normal(yinf_initial*rng.uniform(0., 2.)));

        eval(v);
        pop.push_back(v);
    }
}

void OptimizationEngine::SaveResults(double fitness, double A, double E, double NS, double yinf)
{
    std::ofstream results;

    results.open("results.xy", std::ios::out | std::ios::trunc);

    if(!results.good())
    {
        throw new std::ios_base::failure("Unable to write results.xy");
    }

    results << "#Fitness:" << fitness << ", A: " << A << ", E: " << E << ", NS: " << NS  << ", yinf: " << yinf << std::endl;
    results << "#Time,Temp,Exp,Model"  << std::endl;

    Integrator::Runge23(DataSet, ModData, A, E, NS, yinf);

    for(unsigned int i = 0; i < DataSet->size(); i++)
    {
        results << (*DataSet)[i].Time() <<  "," << (*DataSet)[i].Temp() << "," << (*DataSet)[i].MassFrac() << "," << ModData[i] << std::endl;
    }

    results.close();
}


